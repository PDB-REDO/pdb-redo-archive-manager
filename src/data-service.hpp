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

#include <boost/program_options/variables_map.hpp>

#include <zeep/json/element.hpp>

#include "utilities.hpp"

enum class FileType
{
	ZIP,
	CIF,
	MTZ,
	DATA,
	VERSIONS
};

class data_service
{
  public:
	/// \brief Return the singleton instance of data_service, will init one if it doesn't exist.
	static data_service &instance()
	{
		if (not s_instance)
			s_instance.reset(new data_service);
		return *s_instance;
	}

	void rescan();

	std::string get_pdb_id_for_hash(const std::string &hash) const;

	/// \brief Return the file of type \a type for the hash \a hash returning a tuple containing the istream and name of the download file
	///
	/// \param hash	The hash for this PDB-REDO set of data
	/// \param type The type of the requested file
	/// \returns A tuple containing an std::istream pointer, a name and a content-type string
	std::tuple<std::istream *, std::string, std::string> get_file(const std::string &hash, FileType type);

	/// \brief Return the file of type \a type for the hash \a hash returning a tuple containing the istream and name of the download file
	///
	/// \param hash	The hash for this PDB-REDO set of data
	/// \param type The type of the requested file
	/// \returns A tuple containing an std::istream pointer, a name and a content-type string
	std::tuple<std::istream *, std::string, std::string> get_file(const std::string &hash, const std::string &type)
	{
		if (icompare(type, "zip")) return get_file(hash, FileType::ZIP);
		if (icompare(type, "cif")) return get_file(hash, FileType::CIF);
		if (icompare(type, "mtz")) return get_file(hash, FileType::MTZ);
		if (icompare(type, "data")) return get_file(hash, FileType::DATA);
		if (icompare(type, "versions")) return get_file(hash, FileType::VERSIONS);
		throw std::runtime_error("Invalid file type specified: " + type);
	}

  private:

	data_service();
	data_service(const data_service &) = delete;
	data_service &operator=(const data_service &) = delete;

	std::filesystem::path get_path(const std::string &pdb_id, const std::string &hash, FileType type);

	static std::unique_ptr<data_service> s_instance;

	std::filesystem::path m_pdb_redo_dir;
};