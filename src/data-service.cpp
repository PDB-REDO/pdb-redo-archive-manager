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

#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>

#include <zeep/json/parser.hpp>
#include <zeep/http/reply.hpp>

#include "configuration.hpp"
#include "data-service.hpp"
#include "json2bson.hpp"
#include "utilities.hpp"

namespace fs = std::filesystem;

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

// --------------------------------------------------------------------

std::unique_ptr<data_service> data_service::s_instance;

// --------------------------------------------------------------------

class mongo_pool
{
  public:
	static mongo_pool &instance()
	{
		return *s_instance;
	}

	static void init(const mongocxx::uri &uri)
	{
		s_instance.reset(new mongo_pool(uri));
	}

	auto get()
	{
		return m_pool.acquire();
	}

  private:
	static std::unique_ptr<mongo_pool> s_instance;

	mongo_pool(const mongocxx::uri &uri)
		: m_pool(uri)
	{
	}

	mongocxx::instance m_instance;
	mongocxx::pool m_pool;
};

std::unique_ptr<mongo_pool> mongo_pool::s_instance;

// --------------------------------------------------------------------

data_service::data_service()
{
	auto &config = configuration::instance();

	auto db = config.get("mongo-db-uri");
	mongo_pool::init(mongocxx::uri{db});

	m_pdb_redo_dir = config.get("pdb-redo-dir");
}

void data_service::rescan()
{
	auto &config = configuration::instance();

	// --------------------------------------------------------------------

	auto client = mongo_pool::instance().get();

	client->database("pdb_redo").drop();

	auto coll = client->database("pdb_redo")["entries"];

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
			auto doc = coll.find_one({make_document(kvp("hash", hash))});

			if (doc)
			{
				std::cout << pdb_id << " " << hash << " exists" << std::endl;
				continue;
			}

			std::ifstream versions_file(attic / "versions.json");
			zeep::json::element versions;
			parse_json(versions_file, versions);

			std::ifstream data_file(attic / "data.json");
			zeep::json::element data;
			parse_json(data_file, data);

			using namespace date;
			using namespace std::chrono;

			std::istringstream date_ss{data["properties"]["TIME"].as<std::string>()};
			year_month_day date_ymd{};
			from_stream(date_ss, "%F", date_ymd);
			auto date_dp = sys_days(date_ymd);

			auto r = coll.insert_one(make_document(
				kvp("pdbid", pdb_id),
				kvp("hash", hash),
				kvp("date", bsoncxx::types::b_date{date_dp}),
				kvp("versions", to_bson(versions["data"])),
				kvp("software", to_bson(versions["software"])),
				kvp("properties", to_bson(data["properties"]))
			));

			if (not r)
				std::cerr << "Inserting " << pdb_id << " with hash " << hash << " failed" << std::endl;
		}
		catch (const std::exception &ex)
		{
			std::cerr << ex.what() << std::endl;
		}

		p.consumed(1);
	}

	coll.create_index(make_document(kvp("hash", 1)), make_document(kvp("unique", true)));
}

// --------------------------------------------------------------------

std::tuple<std::istream *, std::string, std::string> data_service::get_file(const std::string &hash, FileType type)
{
	auto client = mongo_pool::instance().get();
	auto coll = client->database("pdb_redo")["entries"];

	auto doc = coll.find_one(make_document(kvp("hash", hash)));
	if (not doc)
		throw zeep::http::not_found;

	auto pdb_id_ele = doc->view()["pdbid"];
	if (not pdb_id_ele)
		throw std::runtime_error("pdbid not found in document!");
	
	std::string pdb_id{pdb_id_ele.get_utf8().operator core::v1::string_view()};

	fs::path path;
	std::string file_name = pdb_id + '_' + hash + '_';
	std::string content_type = "application/octet-stream";

	std::unique_ptr<std::istream> is;

	switch (type)
	{
		case FileType::ZIP:
			break;

		case FileType::CIF:
			content_type = "text/plain";
			path = get_path(pdb_id, hash, FileType::CIF);
			is.reset(new std::ifstream(path));
			file_name += path.filename().string();
			break;

		case FileType::MTZ:
			path = get_path(pdb_id, hash, FileType::MTZ);
			is.reset(new std::ifstream(path));
			file_name += path.filename().string();
			break;

		case FileType::DATA:
			content_type = "application/json";
			path = get_path(pdb_id, hash, FileType::DATA);
			is.reset(new std::ifstream(path));
			file_name += path.filename().string();
			break;

		case FileType::VERSIONS:
			content_type = "application/json";
			path = get_path(pdb_id, hash, FileType::VERSIONS);
			is.reset(new std::ifstream(path));
			file_name += path.filename().string();
			break;

	}

	return { is.release(), file_name, content_type };
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

