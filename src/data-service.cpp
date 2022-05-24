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

#include <filesystem>
#include <fstream>

#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/instance.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>

#include <zeep/json/parser.hpp>

#include "configuration.hpp"
#include "data-service.hpp"
#include "json2bson.hpp"

namespace fs = std::filesystem;

using bsoncxx::builder::basic::kvp;

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
}

void data_service::rescan()
{
	auto &config = configuration::instance();

	fs::path pdbRedoDir{config.get("pdb-redo-dir")};

	std::vector<fs::path> attics;

	for (fs::directory_iterator l1(pdbRedoDir); l1 != fs::directory_iterator(); ++l1)
	{
		if (not l1->is_directory() or l1->path().filename().string().length() != 2)
			continue;
		
		for (fs::directory_iterator l2(l1->path()); l2 != fs::directory_iterator(); ++l2)
		{
			if (not l2->is_directory())
				continue;

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
	}

	// --------------------------------------------------------------------
	
	auto client = mongo_pool::instance().get();
	auto coll = (*client)["pdb_redo"]["attic"];

	for (auto &attic : attics)
	{
		std::string pdb_id = attic.parent_path().parent_path().filename().string();
		std::string hash = attic.filename().string();

		try
		{
			auto doc = coll.find_one({
				bsoncxx::builder::basic::make_document(kvp("hash", hash))
			});

			if (doc)
			{
				std::cout << pdb_id << " " << hash << " exists" << std::endl;
				continue;
			}
			
			std::ifstream versions_file(attic / "versions.json");
			zeep::json::element versions;
			parse_json(versions_file, versions);

			coll.insert_one(to_bson(versions));

		}
		catch(const std::exception& e)
		{
			std::cerr << e.what() << '\n';
		}
		
	}
}
