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

#pragma once

#include <functional>
#include <istream>
#include <memory>

/// \brief A class that stores global configuration information
///
/// Reason to create this class is to have global access to the
/// parameters specified on the command line. And to have an option
/// to remove the dependency on boost::program_options in the future.
class configuration
{
  public:
	~configuration();

	using help_info_function_cb = std::function<void()>;

	/// \brief Singleton access
	static const configuration &instance();

	/// \brief Initialize the configuration object using the global \a argc and \a argv parameters and optionally a call back \a help_cb that provides additional help.
	static const configuration &init(int argc, char * const argv[], std::istream &config_definition_file, help_info_function_cb help_cb = {});

	/// \brief Get the value for option name \a name
	template<typename T = std::string>
	T get(const char* name) const;

	/// \brief Return true if the name option \a name is specified
	bool has(const char *name) const;

  private:

	configuration(int argc, char * const argv[], std::istream &config_definition_file, help_info_function_cb help_cb);

	configuration(const configuration &) = delete;
	configuration& operator=(const configuration &) = delete;

	static std::unique_ptr<configuration> s_instance;

	class configuration_impl *m_impl;
};
