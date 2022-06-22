/*-
 * SPDX-License-Identifier: BSD-2-Filter
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

#include <zeep/json/element.hpp>

#include "utilities.hpp"

// --------------------------------------------------------------------

enum class FileType
{
	ZIP,
	CIF,
	MTZ,
	DATA,
	VERSIONS
};

constexpr const char *mimetype_for_filetype(FileType t)
{
	switch (t)
	{
		case FileType::ZIP: 	return "application/zip";
		case FileType::CIF: 	return "text/plain";
		case FileType::MTZ: 	return "application/octet-stream";
		case FileType::DATA:
		case FileType::VERSIONS:return "application/json";
		default:				throw std::invalid_argument("Invalid file type");
	}
}

constexpr FileType filetype_from_string(std::string_view s)
{
	if (icompare(s, "zip")) return FileType::ZIP;
	if (icompare(s, "cif")) return FileType::CIF;
	if (icompare(s, "mtz")) return FileType::MTZ;
	if (icompare(s, "data")) return FileType::DATA;
	if (icompare(s, "versions")) return FileType::VERSIONS;
	throw std::invalid_argument("Invalid file type");
}

// --------------------------------------------------------------------

enum class PropertyType { String, Number, Boolean };

// --------------------------------------------------------------------

struct Software
{
	std::string name;
	std::vector<std::string> versions;

	template<typename Archive>
	void serialize(Archive& ar, unsigned long version)
	{
		ar & zeep::make_nvp("name", name)
		   & zeep::make_nvp("versions", versions);
	}
};

// --------------------------------------------------------------------

struct DbEntry
{
	std::string pdb_id;
	std::string version_hash;
	std::string date;

	template<typename Archive>
	void serialize(Archive& ar, unsigned long version)
	{
		ar & zeep::make_nvp("id", pdb_id)
		   & zeep::make_nvp("hash", version_hash)
		   & zeep::make_nvp("date", date);
	}
};

// --------------------------------------------------------------------

enum class FilterType { Software, Data };
enum class OperatorType { LT, LE, EQ, GE, GT, NE };

struct Filter
{
	FilterType type;
	std::string subject;
	OperatorType op;
	std::string value;

	template<typename Archive>
	void serialize(Archive& ar, unsigned long version)
	{
		ar & zeep::make_nvp("t", type)
		   & zeep::make_nvp("s", subject)
		   & zeep::make_nvp("o", op)
		   & zeep::make_nvp("v", value);
	}
};

struct Query
{
	bool latest;
	std::vector<Filter> filters;

	template<typename Archive>
	void serialize(Archive& ar, unsigned long version)
	{
		ar & zeep::make_nvp("latest", latest)
		   & zeep::make_nvp("filters", filters);
	}
};

// --------------------------------------------------------------------

class data_service
{
  public:
	/// \brief Wipe databank if it exists and create new based on info in config
	static void reset();

	/// \brief Return the singleton instance of data_service, will init one if it doesn't exist.
	static data_service &instance();

	void rescan();

	/// \brief Return the file of type \a type for the hash \a hash returning a tuple containing the istream and name of the download file
	///
	/// \param id The PDB-REDO ID
	/// \param hash	The hash for this version
	/// \param type The type of the requested file
	/// \returns A tuple containing an std::istream pointer and a file name
	std::tuple<std::istream *, std::string> get_file(const std::string &id, const std::string &hash, FileType type);

	/// \brief Return the file of type \a type for the hash \a hash returning a tuple containing the istream and name of the download file
	///
	/// \param id The PDB-REDO ID
	/// \param hash	The hash for this version
	/// \param type The type of the requested file
	/// \returns A tuple containing an std::istream pointer and a file name
	std::tuple<std::istream *, std::string> get_file(const std::string &id, const std::string &hash, const std::string &type)
	{
		return get_file(id, hash, filetype_from_string(type));
	}

	/// \brief Insert a new PDB-REDO entry
	void insert(const std::string &pdb_id, const std::string &hash, const zeep::json::element &data, const zeep::json::element &versions);

	/// \brief Return the id for the software entry with name \a program and version \a version
	int get_software_id(const std::string &program, const std::string &version) const;

	/// \brief Return the property type for property named \a name
	PropertyType get_property_type(const std::string &name) const
	{
		try
		{
			return m_prop_types.at(name);
		}
		catch(const std::exception& e)
		{
			std::throw_with_nested(std::runtime_error("Undefined property " + name));
		}
	}

	/// \brief Return the list of available programs
	std::vector<Software> get_software() const;

	// --------------------------------------------------------------------
	
	/// \brief Query the data
	std::vector<DbEntry> query_1(const std::string &program, const std::string &version, uint32_t page, uint32_t page_size);
	size_t count_1(const std::string &program, const std::string &version);

	/// \brief Return true if entry exists.
	bool exists(const std::string &pdb_id, const std::string &version_hash) const;

	/// \brief Another query
	std::vector<DbEntry> query(const Query &q, uint32_t page, uint32_t page_size);
	size_t count(const Query &q);

  private:

	data_service();
	data_service(const data_service &) = delete;
	data_service &operator=(const data_service &) = delete;

	std::filesystem::path get_path(const std::string &pdb_id, const std::string &hash, FileType type);

	static std::unique_ptr<data_service> s_instance;

	std::filesystem::path m_pdb_redo_dir;
	std::map<std::string,PropertyType> m_prop_types;
};