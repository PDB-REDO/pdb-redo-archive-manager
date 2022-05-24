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

#include <cassert>

#include <filesystem>
#include <fstream>
#include <iostream>

#include <boost/program_options.hpp>

#include "configuration.hpp"
#include "revision.hpp"

namespace fs = std::filesystem;
namespace po = boost::program_options;

// --------------------------------------------------------------------

class boost_program_options_configuration : public configuration
{
  public:
	boost_program_options_configuration(int argc, char * const argv[], help_info_function_cb help_cb);

	std::string get(const char *name, std::string default_value = {}) const override;
	bool has(const char *name) const override;

  private:

	boost::program_options::variables_map m_vm;
};

boost_program_options_configuration::boost_program_options_configuration(int argc, char * const argv[], help_info_function_cb help_cb)
	: configuration(argc, argv, help_cb)
{
	using namespace std::literals;

	fs::path argv_path(argv[0]);
	std::string program_name = argv_path.filename().string();

	po::options_description visible(program_name + " [options] command"s);
	visible.add_options()
		("help,h",										"Display help message")
		("verbose,v",									"Verbose output")
		("no-daemon,F",									"Do not fork into background")
		("config",		po::value<std::string>(),		"Specify the config file to use")
		("version",										"Print version and exit")
		;
	
	po::options_description config(program_name + R"( config file options)"s);
	
	config.add_options()
		("pdb-redo-dir",	po::value<std::string>(),	"Directory containing PDB-REDO server data")
		("runs-dir",		po::value<std::string>(),	"Directory containing PDB-REDO server run directories")
		("address",			po::value<std::string>()->default_value("0.0.0.0"),	"External address")
		("port",			po::value<uint16_t>()->default_value(10343), "Port to listen to")
		("user,u",			po::value<std::string>(),	"User to run the daemon")
		("db-host",			po::value<std::string>(),	"Database host")
		("db-port",			po::value<std::string>(),	"Database port")
		("db-dbname",		po::value<std::string>(),	"Database name")
		("db-user",			po::value<std::string>(),	"Database user name")
		("db-password",		po::value<std::string>(),	"Database password")
		("mongo-db-uri",	po::value<std::string>(),	"The mongo db connection uri")
		("admin",			po::value<std::string>(),	"Administrators, list of usernames separated by comma")
		("secret",			po::value<std::string>(),	"Secret value, used in signing access tokens");

	po::options_description hidden("hidden options");
	hidden.add_options()
		("command",			po::value<std::string>(),	"Command, one of start, stop, status or reload")
		("debug,d",			po::value<int>(),			"Debug level (for even more verbose output)");

	po::options_description cmdline_options;
	cmdline_options.add(visible).add(config).add(hidden);

	po::positional_options_description p;
	p.add("command", 1);

	po::store(po::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), m_vm);

	fs::path configFile = program_name + ".conf"s;

	if (not fs::exists(configFile) and getenv("HOME") != nullptr)
		configFile = fs::path(getenv("HOME")) / ".config" / configFile;

	if (not fs::exists(configFile) and fs::exists("/etc" / configFile))
		configFile = "/etc" / configFile;
	
	if (m_vm.count("config") != 0)
	{
		configFile = m_vm["config"].as<std::string>();
		if (not fs::exists(configFile))
			throw std::runtime_error("Specified config file cannot be used");
	}
	
	if (fs::exists(configFile))
	{
		po::options_description config_options ;
		config_options.add(config).add(hidden);

		std::ifstream cfgFile(configFile);
		if (cfgFile.is_open())
			po::store(po::parse_config_file(cfgFile, config_options, true), m_vm);
	}
	
	po::notify(m_vm);

	// --------------------------------------------------------------------

	if (m_vm.count("version"))
	{
		write_version_string(std::cout, m_vm.count("verbose"));
		exit(0);
	}

	// --------------------------------------------------------------------

	if (m_vm.count("help"))
	{
		std::cerr << visible << std::endl;

		if (m_help_cb)
			m_help_cb();

		exit(0);
	}
}

std::string boost_program_options_configuration::get(const char *name, std::string default_value) const
{
	if (m_vm.count(name))
		return m_vm[name].as<std::string>();
	return default_value;
}

bool boost_program_options_configuration::has(const char *name) const
{
	return m_vm.count(name);
}

// --------------------------------------------------------------------

std::unique_ptr<configuration> configuration::s_instance;

const configuration &configuration::instance()
{
	assert(s_instance);
	return *s_instance;
}

const configuration &configuration::init(int argc, char * const argv[], help_info_function_cb help_cb)
{
	s_instance.reset(new boost_program_options_configuration(argc, argv, help_cb));
	return *s_instance;
}

configuration::configuration(int argc, char * const argv[], help_info_function_cb help_cb)
	: m_help_cb(help_cb)
{
}

