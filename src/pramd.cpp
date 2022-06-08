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

#include <zeep/config.hpp>

// #include <functional>
// #include <tuple>
// #include <thread>
// #include <condition_variable>
#include <fstream>

// #include <boost/algorithm/string.hpp>

// #include <boost/uuid/uuid.hpp>
// #include <boost/uuid/uuid_generators.hpp>
// #include <boost/uuid/uuid_io.hpp>

// #include <boost/date_time/posix_time/posix_time.hpp>
// #include <boost/date_time/gregorian/gregorian.hpp>
// #include <boost/date_time/local_time/local_time.hpp>
// #include <boost/random/random_device.hpp>

// #include <zeep/crypto.hpp>
#include <zeep/http/daemon.hpp>
#include <zeep/http/rest-controller.hpp>
#include <zeep/http/html-controller.hpp>
#include <zeep/http/security.hpp>
#include <zeep/http/login-controller.hpp>
// #include <zeep/http/uri.hpp>

// #include <pqxx/pqxx>

// #include "user-service.hpp"
// #include "run-service.hpp"

#include "configuration.hpp"
#include "data-service.hpp"
#include "db-connection.hpp"

#include "mrsrc.hpp"

namespace zh = zeep::http;
namespace fs = std::filesystem;
namespace ba = boost::algorithm;
namespace pt = boost::posix_time;

using json = zeep::json::element;

// --------------------------------------------------------------------

#define APP_NAME "pramd"

class api_rest_controller : public zh::rest_controller
{
  public:
	api_rest_controller()
		: zh::rest_controller("archive")
	{
		// get a file
		map_get_request("file/{id}/{hash}/{type}", &api_rest_controller::get_file, "id", "hash", "type");
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

  private:
	fs::path m_pdb_redo_dir;
};

// --------------------------------------------------------------------

class pram_html_controller : public zh::html_controller
{
  public:
	pram_html_controller()
	{
		mount("", &pram_html_controller::welcome);

		mount("search", &pram_html_controller::search);

		mount("{css,scripts,fonts,images}/", &pram_html_controller::handle_file);
	}

	void welcome(const zh::request& request, const zh::scope& scope, zh::reply& reply);
	void search(const zh::request& request, const zh::scope& scope, zh::reply& reply);
};

void pram_html_controller::welcome(const zh::request& request, const zh::scope& scope, zh::reply& reply)
{
	get_template_processor().create_reply_from_template("index.html", scope, reply);
}

void pram_html_controller::search(const zh::request& request, const zh::scope& scope, zh::reply& reply)
{
	zh::scope sub(scope);

	std::string
		program = request.get_parameter("program"),
		version = request.get_parameter("version");

	auto software = data_service::instance().get_software();
	zeep::json::element se;
	to_element(se, software);
	sub.put("software", se);

	sub.put("program", program);
	sub.put("version", version);
	sub.put("programIx", std::find_if(software.begin(), software.end(), [program](Software &sw) { return sw.name == program; }) - software.begin());

	get_template_processor().create_reply_from_template("search", sub, reply);
}

// --------------------------------------------------------------------

int a_main(int argc, char* const argv[])
{
	using namespace std::literals;

	int result = 0;

#if USE_RSRC
	mrsrc::istream config_templ("configuration.json");
#endif

	auto &config = configuration::init(argc, argv, config_templ, []()
	{
		std::cerr << R"(Command should be either:

  start     start a new server
  stop      start a running server
  status    get the status of a running server
  reload    restart a running server with new options
			 )" << std::endl;
	});

	// --------------------------------------------------------------------

	if (not config.has("command"))
	{
		std::cerr << "No command specified" << std::endl;
		exit(1);
	}

	db_connection::init();

	auto command = config.get("command");

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

		return s;
	}, APP_NAME );

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
