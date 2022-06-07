/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 NKI/AVL, Netherlands Cancer Institute
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <chrono>
#include <filesystem>
#include <fstream>

#include <date/date.h>

#include <libpq-fe.h>
#include <pqxx/pqxx>

#include <zeep/json/parser.hpp>
#include <zeep/http/reply.hpp>

#include "mrsrc.hpp"

#include "configuration.hpp"
#include "data-service.hpp"
#include "db-connection.hpp"
#include "utilities.hpp"

namespace fs = std::filesystem;

// --------------------------------------------------------------------

std::unique_ptr<data_service> data_service::s_instance;

// --------------------------------------------------------------------

data_service::data_service()
{
	auto &config = configuration::instance();

	m_pdb_redo_dir = config.get("pdb-redo-dir");

	// the data.json schema
	mrsrc::istream schema_s("data.json.schema");
	if (not schema_s)
		throw std::runtime_error("Missing resource");

	zeep::json::element schema;
	parse_json(schema_s, schema);

	auto prop_schema = schema["definitions"]["Properties"]["properties"];

	for (auto prop_it = prop_schema.begin(); prop_it != prop_schema.end(); ++prop_it)
	{
		auto name = prop_it.key();
		auto &value = prop_it.value()["type"];

		if (value.type() == zeep::json::element::value_type::array)
		{
			for (auto &type : value)
			{
				auto type_s = type.as<std::string>();
				if (type_s == "string")
					m_prop_types[name] = PropertyType::String;
				else if (type_s == "number")
					m_prop_types[name] = PropertyType::Number;
				else if (type_s == "boolean")
					m_prop_types[name] = PropertyType::Boolean;
				else
					continue;
				break;
			}
		}
		else if (value.type() == zeep::json::element::value_type::string)
		{
			auto type_s = value.as<std::string>();
			if (type_s == "string")
				m_prop_types[name] = PropertyType::String;
			else if (type_s == "number")
				m_prop_types[name] = PropertyType::Number;
			else if (type_s == "boolean")
				m_prop_types[name] = PropertyType::Boolean;
		}
		
		if (not m_prop_types.count(name))
			throw std::runtime_error("Property " + name + " has unknown type");
	}
}

// --------------------------------------------------------------------

std::vector<Software> data_service::get_software() const
{
	std::vector<Software> result;

	pqxx::work tx(db_connection::instance());

	for (const auto &[name, version] : tx.stream<std::string,std::string>("SELECT name, version FROM software ORDER BY name, version"))
	{
		if (result.empty() or result.back().name != name)
		{
			result.emplace_back(Software{name, { version }});
			continue;
		}

		result.back().versions.emplace_back(version);
	}

	return result;
}

// --------------------------------------------------------------------

void data_service::reset()
{
	using namespace std::literals;

	auto &config = configuration::instance();

	// --------------------------------------------------------------------
	// Don't use libpqxx here, it is too limited

	std::ostringstream connectionString;
	std::string dbuser;

	for (auto opt: { "db-host", "db-port", "db-dbname", "db-user", "db-password" })
	{
		if (not config.has(opt))
			continue;
		
		if (strcmp(opt, "db-user") == 0)
			dbuser = config.get(opt);

		connectionString << (opt + 3) << '=' << config.get(opt) << ' ';
	}

	auto connection = PQconnectdb(connectionString.str().c_str());
	if (connection == nullptr)
		throw std::runtime_error("Unable to connect to database");

	if (PQstatus(connection) != CONNECTION_OK)
		throw std::runtime_error(PQerrorMessage(connection));
	
	// --------------------------------------------------------------------
	
	// the schema
	mrsrc::rsrc schema("db-schema.sql");
	if (not schema)
		throw std::runtime_error("Missing schema resource");

	std::string sql(schema.data(), schema.size());
	if (not dbuser.empty())
	{
		std::string::size_type i = 0;
		while ((i = sql.find("${owner}", i)) != std::string::npos)
			sql.replace(i, strlen("${owner}"), dbuser);
	}

	auto r = PQexec(connection, sql.c_str());
	if (r == nullptr)
		throw std::runtime_error(PQerrorMessage(connection));

	if (PQresultStatus(r) != PGRES_COMMAND_OK)
		throw std::runtime_error(PQresultErrorMessage(r));

	PQfinish(connection);
}

void data_service::rescan()
{
	reset();

	auto &config = configuration::instance();

	// --------------------------------------------------------------------

	fs::path pdbRedoDir{config.get("pdb-redo-dir")};

	size_t n = 0;
	for (fs::directory_iterator l1(pdbRedoDir); l1 != fs::directory_iterator(); ++l1)
	{
		if (l1->is_directory() and l1->path().filename().string().length() == 2)
			++n;
	}

	progress p0("scanning", n);

	std::vector<fs::path> attics;

	for (fs::directory_iterator l1(pdbRedoDir); l1 != fs::directory_iterator(); ++l1)
	{
		if (not l1->is_directory() or l1->path().filename().string().length() != 2)
			continue;

		for (fs::directory_iterator l2(l1->path()); l2 != fs::directory_iterator(); ++l2)
		{
			if (not l2->is_directory())
				continue;

			p0.message(l2->path().filename().string());

			fs::path attic = l2->path() / "attic";

			if (not fs::is_directory(attic))
				continue;

			for (fs::directory_iterator l3(attic); l3 != fs::directory_iterator(); ++l3)
			{
				if (not l3->is_directory() or not fs::exists(l3->path() / "versions.json"))
					continue;

				attics.emplace_back(l3->path());
			}
		}

		p0.consumed(1);
	}

	// --------------------------------------------------------------------

	progress p("Importing entries", attics.size());

	for (auto &attic : attics)
	{
		std::string pdb_id = attic.parent_path().parent_path().filename().string();
		std::string hash = attic.filename().string();

		p.message(pdb_id);

		try
		{
			std::ifstream versions_file(attic / "versions.json");
			zeep::json::element versions;
			parse_json(versions_file, versions);

			std::ifstream data_file(attic / "data.json");
			zeep::json::element data;
			parse_json(data_file, data);

			insert(pdb_id, hash, data, versions);
		}
		catch (const std::exception &ex)
		{
			std::cerr << ex.what() << std::endl;
		}

		p.consumed(1);
	}
}

// --------------------------------------------------------------------

void data_service::insert(const std::string &pdb_id, const std::string &hash, const zeep::json::element &data, const zeep::json::element &versions)
{
	pqxx::work tx(db_connection::instance());

	using namespace date;
	using namespace std::chrono;


	// std::istringstream date_ss{data["properties"]["TIME"].as<std::string>()};
	// year_month_day date_ymd{};
	// from_stream(date_ss, "%F", date_ymd);
	// auto date_dp = sys_days(date_ymd);

	auto &versions_data = versions["data"];
	auto coordinates_revision_date_pdb = versions_data["coordinates_revision_date_pdb"].as<std::string>();
	auto coordinates_revision_major_mmCIF = versions_data["coordinates_revision_major_mmCIF"].as<std::string>();
	auto coordinates_revision_minor_mmCIF = versions_data["coordinates_revision_minor_mmCIF"].as<std::string>();
	auto coordinates_edited = versions_data["coordinates_edited"].as<bool>();
	auto reflections_revision = versions_data["reflections_revision"].as<std::string>();
	auto reflections_edited = versions_data["reflections_edited"].as<bool>();

	auto r = tx.exec1(R"(
		INSERT INTO dbentry (pdb_id, version_hash, coordinates_revision_date_pdb, coordinates_revision_major_mmCIF, coordinates_revision_minor_mmCIF,
			coordinates_edited, reflections_revision, reflections_edited)
		VALUES ()" +
			 tx.quote(pdb_id) + ", " +
			 tx.quote(hash) + ", " +
			 tx.quote(coordinates_revision_date_pdb) + ", " +
			 tx.quote(coordinates_revision_major_mmCIF) + ", " +
			 tx.quote(coordinates_revision_minor_mmCIF) + ", " +
			 tx.quote(coordinates_edited) + ", " +
			 tx.quote(reflections_revision) + ", " +
			 tx.quote(reflections_edited) + ") RETURNING id");

	auto id = r.at("id").as<int>();

	auto &software = versions["software"];
	for (auto software_it = software.begin(); software_it != software.end(); ++software_it)
	{
		auto program = software_it.key();
		auto version = software_it.value()["version"].as<std::string>();
		auto used = software_it.value()["used"].as<bool>();

		int software_id;

		try
		{
			pqxx::subtransaction txs(tx);
			auto r = txs.exec1("SELECT id FROM software WHERE name = " + tx.quote(program) + " AND version = " + tx.quote(version));
			std::tie(software_id) = r.as<int>();
			txs.commit();
		}
		catch (const std::exception &ex)
		{
			pqxx::subtransaction txs(tx);
			auto r = txs.exec1("INSERT INTO software (name, version) VALUES (" + tx.quote(program) + ", " + tx.quote(version) + ") RETURNING id");
			std::tie(software_id) = r.as<int>();
			txs.commit();
		}
		
		pqxx::subtransaction txs2(tx);
		txs2.exec("INSERT INTO dbentry_software (dbentry_id, software_id, used) VALUES (" + tx.quote(id) + ", " + tx.quote(software_id) + ", " + tx.quote(used) + ")");
		txs2.commit();
	}

	auto &properties = data["properties"];
	for (auto property_it = properties.begin(); property_it != properties.end(); ++property_it)
	{
		auto name = property_it.key();
		auto value = property_it.value();

		if (value.is_null())
			continue;

		int property_id;

		try
		{
			pqxx::subtransaction txs(tx);
			auto r = txs.exec1("SELECT id FROM property WHERE name = " + tx.quote(name));
			std::tie(property_id) = r.as<int>();
			txs.commit();
		}
		catch (const std::exception &ex)
		{
			pqxx::subtransaction txs(tx);
			auto r = txs.exec1("INSERT INTO property (name) VALUES (" + tx.quote(name) + ") RETURNING id");
			std::tie(property_id) = r.as<int>();
			txs.commit();
		}

		pqxx::subtransaction txs(tx);
		switch (get_property_type(name))
		{
			case PropertyType::String:
				txs.exec("INSERT INTO dbentry_property_string (dbentry_id, property_id, value) VALUES (" + tx.quote(id) + ", " + tx.quote(property_id) + ", " + tx.quote(value.as<std::string>()) + ")");
				break;

			case PropertyType::Number:
				txs.exec("INSERT INTO dbentry_property_number (dbentry_id, property_id, value) VALUES (" + tx.quote(id) + ", " + tx.quote(property_id) + ", " + tx.quote(value.as<long double>()) + ")");
				break;

			case PropertyType::Boolean:
				txs.exec("INSERT INTO dbentry_property_boolean (dbentry_id, property_id, value) VALUES (" + tx.quote(id) + ", " + tx.quote(property_id) + ", " + tx.quote(value.as<bool>()) + ")");
				break;

			default:
				std::runtime_error("Error, unknown property type for " + name);
		}

		txs.commit();
	}

	tx.commit();

}

int data_service::get_software_id(const std::string &program, const std::string &version) const
{
	pqxx::work tx(db_connection::instance());

	int result = -1;

	try
	{
		auto r = tx.exec1("SELECT id FROM software WHERE name == " + tx.quote(program) + " AND version = " + tx.quote(version));
		std::tie(result) = r.as<int>();
	}
	catch (const std::exception &ex)
	{
		auto r = tx.exec1("INSERT INTO software (name, version) VALUES (" + tx.quote(program) + ", " + tx.quote(version) + "RETURNING id");
		std::tie(result) = r.as<int>();
	}
	
	tx.commit();

	return result;
}

// --------------------------------------------------------------------

std::tuple<std::istream *, std::string> data_service::get_file(const std::string &id, const std::string &hash, FileType type)
{
	fs::path path;
	std::string file_name = id + '_' + hash + '_';

	std::unique_ptr<std::istream> is;

	switch (type)
	{
		case FileType::ZIP:
			break;

		case FileType::CIF:
			path = get_path(id, hash, FileType::CIF);
			is.reset(new std::ifstream(path));
			file_name += path.filename().string();
			break;

		case FileType::MTZ:
			path = get_path(id, hash, FileType::MTZ);
			is.reset(new std::ifstream(path));
			file_name += path.filename().string();
			break;

		case FileType::DATA:
			path = get_path(id, hash, FileType::DATA);
			is.reset(new std::ifstream(path));
			file_name += path.filename().string();
			break;

		case FileType::VERSIONS:
			path = get_path(id, hash, FileType::VERSIONS);
			is.reset(new std::ifstream(path));
			file_name += path.filename().string();
			break;

	}

	return { is.release(), file_name };
}

fs::path data_service::get_path(const std::string &pdb_id, const std::string &hash, FileType type)
{
	switch (type)
	{
		case FileType::CIF:
			return m_pdb_redo_dir / pdb_id.substr(1, 2) / pdb_id / "attic" / hash / "final.cif.gz";

		case FileType::MTZ:
			return m_pdb_redo_dir / pdb_id.substr(1, 2) / pdb_id / "attic" / hash / "final.mtz.gz";

		case FileType::DATA:
			return m_pdb_redo_dir / pdb_id.substr(1, 2) / pdb_id / "attic" / hash / "data.json";

		case FileType::VERSIONS:
			return m_pdb_redo_dir / pdb_id.substr(1, 2) / pdb_id / "attic" / hash / "versions.json";

		default:
			throw std::invalid_argument("zips are special");
	}
}

