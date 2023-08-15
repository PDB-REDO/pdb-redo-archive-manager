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

#include <date/date.h>
#include <fstream>
#include <iostream>

#include <zeep/config.hpp>

#include <zeep/http/daemon.hpp>
#include <zeep/http/html-controller.hpp>
#include <zeep/http/login-controller.hpp>
#include <zeep/http/rest-controller.hpp>
#include <zeep/http/security.hpp>

#include <mcfp/mcfp.hpp>

#include "data-service.hpp"
#include "db-connection.hpp"

#include "mrsrc.hpp"
#include "revision.hpp"

namespace zh = zeep::http;
namespace fs = std::filesystem;

using json = zeep::json::element;

const int
	kPageSize = 10;

// --------------------------------------------------------------------

#define APP_NAME "pramd"

class software_expression_utility_object : public zh::expression_utility_object<software_expression_utility_object>
{
  public:
	static constexpr const char *name() { return "software"; }

	virtual zh::object evaluate(const zh::scope &scope, const std::string &methodName,
		const std::vector<zh::object> &parameters) const
	{
		zh::object result;

		if (methodName == "indexOf" and parameters.size() == 1)
		{
			try
			{
				auto &ds = data_service::instance();
				auto software = ds.get_software();

				auto sw = parameters.front().as<std::string>();

				for (size_t ix = 0; ix < software.size(); ++ix)
				{
					if (software[ix].name != sw)
						continue;
					
					result = ix;
					break;
				}
			}
			catch (const std::exception &e)
			{
				std::cerr << "Error getting post by ID: " << e.what() << std::endl;
			}
		}

		return result;
	}

} s_software_expression_instance;

// --------------------------------------------------------------------

class api_rest_controller : public zh::rest_controller
{
  public:
	api_rest_controller()
		: zh::rest_controller("v1")
	{
		// get a file
		map_get_request("file/{id}/{hash}/{type}", &api_rest_controller::get_file, "id", "hash", "type");

		// query construction support

		// return list of all software
		map_get_request("q/software", &api_rest_controller::get_all_software);

		// return specific software item
		map_get_request("q/software/{name}", &api_rest_controller::get_software, "name");

		// return list of all properties
		map_get_request("q/property", &api_rest_controller::get_all_properties);

		// return specific property item
		map_get_request("q/property/{name}", &api_rest_controller::get_property, "name");

		// Actual query support

		// return all results
		map_post_request("q/query", &api_rest_controller::query_all, "query");

		// return paged results
		map_post_request("q/query/{page}", &api_rest_controller::query_page, "query", "page");

		// return the count(*) for query
		map_post_request("q/count", &api_rest_controller::query_count, "query");
	}

	std::vector<Software> get_all_software()
	{
		auto &ds = data_service::instance();
		return ds.get_software();
	}

	Software get_software(const std::string &name)
	{
		auto &ds = data_service::instance();
		for (auto &sw : ds.get_software())
		{
			if (sw.name == name)
				return sw;
		}
		throw zh::not_found;
	}

	std::vector<Property> get_all_properties()
	{
		auto &ds = data_service::instance();
		return ds.get_properties();
	}

	Property get_property(const std::string &name)
	{
		auto &ds = data_service::instance();
		for (auto &p : ds.get_properties())
		{
			if (p.name == name)
				return p;
		}
		throw zh::not_found;
	}

	zh::reply get_file(const std::string &id, const std::string &hash, const std::string &type)
	{
		auto &&[file, name] = data_service::instance().get_file(id, hash, type);
		auto content_type = mimetype_for_filetype(filetype_from_string(type));

		zh::reply rep{zh::ok};
		rep.set_content(file, content_type);
		rep.set_header("content-disposition", "attachement; filename = \"" + name + '"');

		return rep;
	}

	std::vector<DbEntry> query_all(Query q)
	{
		return {};
	}

	std::vector<DbEntry> query_page(Query q, int page)
	{
		auto &ds = data_service::instance();
		return ds.query(q, page, kPageSize);
	}

	size_t query_count(Query q)
	{
		auto &ds = data_service::instance();
		return ds.count(q);
	}

  private:
	fs::path m_pdb_redo_dir;
};

// --------------------------------------------------------------------

class pram_html_controller : public zh::html_controller
{
  public:
	pram_html_controller()
		: m_next_id(1)
	{
		mount("", &pram_html_controller::welcome);
		mount("export", &pram_html_controller::export_results);

		mount("entries-table", &pram_html_controller::entries_table);

		mount("program-filter", &pram_html_controller::program_filter);
		mount("property-filter", &pram_html_controller::property_filter);

		mount("{css,scripts,fonts,images}/", &pram_html_controller::handle_file);
	}

	void welcome(const zh::request &request, const zh::scope &scope, zh::reply &reply);

	void export_results(const zh::request &request, const zh::scope &scope, zh::reply &reply);

	void entries_table(const zh::request &request, const zh::scope &scope, zh::reply &reply);
	void program_filter(const zh::request &request, const zh::scope &scope, zh::reply &reply);
	void property_filter(const zh::request &request, const zh::scope &scope, zh::reply &reply);

	std::atomic<int> m_next_id;
};

void pram_html_controller::welcome(const zh::request &request, const zh::scope &scope, zh::reply &reply)
{
	using json = zeep::json::element;

	zh::scope sub(scope);
	auto &ds = data_service::instance();

	auto software = ds.get_software();
	json se;
	to_element(se, software);
	sub.put("software", se);

	auto properties = ds.get_properties();
	json pe;
	to_element(pe, properties);
	sub.put("properties", pe);

	get_template_processor().create_reply_from_template("index", sub, reply);
}

void pram_html_controller::export_results(const zh::request &request, const zh::scope &scope, zh::reply &reply)
{
	using json = zeep::json::element;
    using namespace date;
    using namespace std::chrono;

	auto &ds = data_service::instance();

	json jq;
	parse_json(request.get_parameter("query"), jq);

    auto tp = system_clock::now();
    auto dp = floor<days>(tp);
    auto ymd = year_month_day{dp};
    auto time = make_time(floor<seconds>(tp-dp));

	std::ostringstream ss;
	ss << ymd << ' ' << time << " UTC";

	json content{
		{ "date", ss.str() },
		{ "query", jq },
		{ "entries", {} }
	};

	Query q;
	from_element(jq, q);

	auto dbentries = ds.query(q, 0, std::numeric_limits<uint32_t>::max());
	to_element(content["entries"], dbentries);

	std::unique_ptr<std::iostream> os(new std::stringstream);

	*os << content;

	reply.set_content(os.release(), "application/json");

	std::string filename = "pdb-archive-query-result-(" + ss.str() + ").json";
	std::string::size_type i = 0;
	while ((i = filename.find(':', i)) != std::string::npos)
		filename[i] = '.';

	reply.set_header("Content-Disposition", R"(attachment; filename=")" + filename + "\"");
}

void pram_html_controller::entries_table(const zh::request &request, const zh::scope &scope, zh::reply &reply)
{
	using json = zeep::json::element;

	zh::scope sub(scope);

	auto &ds = data_service::instance();
	int page = request.get_parameter("page", 0);

	json jq;
	parse_json(request.get_parameter("query"), jq);

	Query q;
	from_element(jq, q);

	auto dbentries = ds.query(q, page, kPageSize);

	json entries;
	to_element(entries, dbentries);

	sub.put("entries", entries);

	return get_template_processor().create_reply_from_template("index::entries-table-fragment", sub, reply);
}

void pram_html_controller::program_filter(const zh::request &request, const zh::scope &scope, zh::reply &reply)
{
	using json = zeep::json::element;

	zh::scope sub(scope);
	auto &ds = data_service::instance();

	auto software = ds.get_software();
	json se;
	to_element(se, software);
	sub.put("software", se);

	auto program = request.get_parameter("program");
	sub.put("program", program);

	sub.put("filter-id", m_next_id++);

	return get_template_processor().create_reply_from_template("search-elements::program-filter", sub, reply);
}

void pram_html_controller::property_filter(const zh::request &request, const zh::scope &scope, zh::reply &reply)
{
	using json = zeep::json::element;

	zh::scope sub(scope);
	auto &ds = data_service::instance();

	auto properties = ds.get_properties();
	json se;
	to_element(se, properties);
	sub.put("properties", se);

	auto property = request.get_parameter("property");
	sub.put("property", property);

	sub.put("property-type", ds.get_property_type(property));

	sub.put("filter-id", m_next_id++);

	return get_template_processor().create_reply_from_template("search-elements::property-filter", sub, reply);
}

// --------------------------------------------------------------------

int a_main(int argc, char *const argv[])
{
	using namespace std::literals;

	int result = 0;

	auto &config = mcfp::config::instance();

	config.init(
		"usage: pramd [options] command",
		mcfp::make_option("help,h", "Display help message"),
		mcfp::make_option("version", "Show version information"),
		mcfp::make_option("verbose,v", "Verbose output"),
		mcfp::make_option("no-daemon,F", "Do not fork into background"),
		mcfp::make_option<std::string>("pdb-redo-dir", "Directory containing PDB-REDO server data"),
		mcfp::make_option<std::string>("runs-dir", "Directory containing PDB-REDO server run directories"),
		mcfp::make_option<std::string>("address", "0.0.0.0", "External address"),
		mcfp::make_option<uint16_t>("port", 10343, "Port to listen to"),
		mcfp::make_option<std::string>("context", "The base part of the URL in case this server is behind a reverse proxy"),
		mcfp::make_option<std::string>("user,u", "User to run the daemon"),
		mcfp::make_option<std::string>("db-host", "Database host"),
		mcfp::make_option<std::string>("db-port", "Database port"),
		mcfp::make_option<std::string>("db-dbname", "Database name"),
		mcfp::make_option<std::string>("db-user", "Database user name"),
		mcfp::make_option<std::string>("db-password", "Database password"));

	std::error_code ec;
	config.parse(argc, argv, ec);
	if (ec)
		throw std::runtime_error("Error parsing arguments: " + ec.message());

	if (config.has("version"))
	{
		write_version_string(std::cout, config.has("verbose"));
		exit(0);
	}

	if (config.has("help"))
	{
		std::cerr << config << std::endl;
		exit(config.has("help") ? 0 : 1);
	}

	config.parse_config_file("config", "pramd.conf", { fs::current_path().string(), "/etc/" }, ec);
	if (ec)
		throw std::runtime_error("Error parsing config file: " + ec.message());

	// --------------------------------------------------------------------

	if (config.has("help") or config.operands().empty())
	{
		std::cerr << config << std::endl
				  << R"(
Command should be either:

  start     start a new server
  stop      start a running server
  status    get the status of a running server
  reload    restart a running server with new options
  reinit    re-initialise the database
  rescan    update the database with new entries
			 )" << std::endl;
		exit(config.has("help") ? 0 : 1);
	}

	// --------------------------------------------------------------------

	db_connection::init();

	auto command = config.operands().front();

	if (command == "reinit")
	{
		data_service::reset();
		return 0;
	}

	if (not config.has("pdb-redo-dir"))
	{
		std::cerr << "Missing pdb-redo-dir option" << std::endl;
		exit(1);
	}

	if (command == "rescan")
	{
		data_service::instance().rescan();
		return 0;
	}

	zh::daemon server([&config]()
		{
		auto s = new zeep::http::server{};

		if (config.has("context"))
			s->set_context_name(config.get("context"));

		s->add_error_handler(new db_error_handler());

#ifndef NDEBUG
		s->set_template_processor(new zeep::http::file_based_html_template_processor("docroot"));
#else
		s->set_template_processor(new zeep::http::rsrc_based_html_template_processor());
#endif

		s->add_controller(new pram_html_controller());
		s->add_controller(new api_rest_controller());

		return s; },
		APP_NAME);

	if (command == "start")
	{
		std::string address = config.get("address");
		uint16_t port = config.get<uint16_t>("port");
		std::string user = config.get("user");

		if (address.find(':') != std::string::npos)
			std::cout << "starting server at http://[" << address << "]:" << port << '/' << std::endl;
		else
			std::cout << "starting server at http://" << address << ':' << port << '/' << std::endl;

		if (config.has("no-daemon"))
			result = server.run_foreground(address, port);
		else
			result = server.start(address, port, 2, 1, user);
	}
	else if (command == "stop")
		result = server.stop();
	else if (command == "status")
		result = server.status();
	else if (command == "reload")
		result = server.reload();
	else
	{
		std::cerr << "Invalid command" << std::endl;
		result = 1;
	}

	return result;
}

// --------------------------------------------------------------------

// recursively print exception whats:
void print_what(const std::exception &e)
{
	std::cerr << e.what() << std::endl;
	try
	{
		std::rethrow_if_nested(e);
	}
	catch (const std::exception &nested)
	{
		std::cerr << " >> ";
		print_what(nested);
	}
}

// --------------------------------------------------------------------

int main(int argc, char *const argv[])
{
	int result = 0;

	try
	{
		result = a_main(argc, argv);
	}
	catch (const std::exception &ex)
	{
		print_what(ex);
		exit(1);
	}

	return result;
}
