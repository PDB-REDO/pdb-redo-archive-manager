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

#include <zeep/json/parser.hpp>

#include "configuration.hpp"
#include "revision.hpp"

namespace fs = std::filesystem;
namespace po = boost::program_options;

using json = zeep::json::element;

// --------------------------------------------------------------------

class configuration_impl
{
  public:
	virtual ~configuration_impl() = default;

	virtual bool has(const char *name) const = 0;
};

// --------------------------------------------------------------------

class boost_program_options_configuration : public configuration_impl
{
  public:
	boost_program_options_configuration(int argc, char *const argv[], std::istream &config_definition_file, configuration::help_info_function_cb help_cb);

	bool has(const char *name) const override
	{
		return m_vm.count(name);
	}

	template <typename T>
	T get(const char *name) const
	{
		return m_vm[name].as<T>();
	}

  private:
	boost::program_options::variables_map m_vm;
};

boost_program_options_configuration::boost_program_options_configuration(int argc, char *const argv[],
	std::istream &config_definition_file, configuration::help_info_function_cb help_cb)
{
	using namespace std::literals;

	json config_template;
	parse_json(config_definition_file, config_template);

	fs::path argv_path(argv[0]);
	std::string program_name = argv_path.filename().string();

	std::string desc = config_template["desc"].as<std::string>();
	if (desc.empty())
		desc = program_name + " [options]";
	else
	{
		std::string::size_type i;
		while ((i = desc.find("${program_name}")) != std::string::npos)
			desc.replace(i, strlen("${program_name}"), program_name);
	}

	po::options_description visible(desc);
	po::options_description config(program_name + " config file options");
	po::options_description hidden("hidden options");
	po::positional_options_description p;

	for (auto &&[name, od] : {
			 std::tuple<const char *, po::options_description &>{"visible", std::ref(visible)},
			 std::tuple<const char *, po::options_description &>{"config", std::ref(config)},
			 std::tuple<const char *, po::options_description &>{"hidden", std::ref(hidden)},
		 })
	{
		for (auto opt : config_template[name])
		{
			std::string o_name = opt["name"].as<std::string>();
			std::string o_desc = opt["desc"].as<std::string>();
			std::string o_type = opt["type"].as<std::string>();
			auto o_default = opt["default"];
			auto o_position = opt["position"];

			boost::shared_ptr<po::option_description> option;

			if (o_default.empty())
			{
				if (o_type == "switch")
					option.reset(new po::option_description(o_name.c_str(), new po::untyped_value(true), o_desc.c_str()));
				else if (o_type == "string")
					option.reset(new po::option_description(o_name.c_str(), po::value<std::string>(), o_desc.c_str()));
				else if (o_type == "int")
					option.reset(new po::option_description(o_name.c_str(), po::value<int>(), o_desc.c_str()));
				else if (o_type == "uint")
					option.reset(new po::option_description(o_name.c_str(), po::value<unsigned int>(), o_desc.c_str()));
				else if (o_type == "int16_t")
					option.reset(new po::option_description(o_name.c_str(), po::value<int16_t>(), o_desc.c_str()));
				else if (o_type == "uint16_t")
					option.reset(new po::option_description(o_name.c_str(), po::value<uint16_t>(), o_desc.c_str()));
				else if (o_type == "float")
					option.reset(new po::option_description(o_name.c_str(), po::value<float>(), o_desc.c_str()));
				else if (o_type == "double")
					option.reset(new po::option_description(o_name.c_str(), po::value<double>(), o_desc.c_str()));
			}
			else
			{
				if (o_type == "string")
					option.reset(new po::option_description(o_name.c_str(), po::value<std::string>()->default_value(o_default.as<std::string>()), o_desc.c_str()));
				else if (o_type == "int")
					option.reset(new po::option_description(o_name.c_str(), po::value<int>()->default_value(o_default.as<int>()), o_desc.c_str()));
				else if (o_type == "uint")
					option.reset(new po::option_description(o_name.c_str(), po::value<unsigned int>()->default_value(o_default.as<unsigned int>()), o_desc.c_str()));
				else if (o_type == "int16_t")
					option.reset(new po::option_description(o_name.c_str(), po::value<int16_t>()->default_value(o_default.as<int16_t>()), o_desc.c_str()));
				else if (o_type == "uint16_t")
					option.reset(new po::option_description(o_name.c_str(), po::value<uint16_t>()->default_value(o_default.as<uint16_t>()), o_desc.c_str()));
				else if (o_type == "float")
					option.reset(new po::option_description(o_name.c_str(), po::value<float>()->default_value(o_default.as<float>()), o_desc.c_str()));
				else if (o_type == "double")
					option.reset(new po::option_description(o_name.c_str(), po::value<double>()->default_value(o_default.as<double>()), o_desc.c_str()));
			}

			if (not option)			
				throw std::runtime_error("Unimplemented option type for option " + o_name + " with type " + o_type);

			od.add(option);

			if (not o_position.empty())
				p.add(o_name.c_str(), o_position.as<int>());
		}
	}

	visible.add_options()
		("config", po::value<std::string>(), "Specify the config file to use")
		("help,h", "Display help message")
		("verbose,v", "Verbose output")
		("version", "Print version and exit");

	po::options_description cmdline_options;
	cmdline_options.add(visible).add(config).add(hidden);

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
		po::options_description config_options;
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

		if (help_cb)
			help_cb();

		exit(0);
	}
}

// --------------------------------------------------------------------

std::unique_ptr<configuration> configuration::s_instance;

const configuration &configuration::instance()
{
	assert(s_instance);
	return *s_instance;
}

const configuration &configuration::init(int argc, char *const argv[], std::istream &config_definition_file, help_info_function_cb help_cb)
{
	s_instance.reset(new configuration(argc, argv, config_definition_file, help_cb));
	return *s_instance;
}

configuration::configuration(int argc, char *const argv[], std::istream &config_definition_file, help_info_function_cb help_cb)
	: m_impl(new boost_program_options_configuration(argc, argv, config_definition_file, help_cb))
{
}

configuration::~configuration()
{
	delete m_impl;
}

bool configuration::has(const char *name) const
{
	return m_impl->has(name);
}

template <typename T>
T configuration::get(const char *name) const
{
	return static_cast<boost_program_options_configuration *>(m_impl)->get<T>(name);
}

template std::string configuration::get(const char *) const;
template int configuration::get(const char *) const;
template uint16_t configuration::get(const char *) const;