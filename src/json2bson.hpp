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

#include <zeep/json/element.hpp>

#include <bsoncxx/builder/basic/document.hpp>

namespace zeep::json
{

inline bsoncxx::document::value to_bson(const element &e)
{
	assert(e.type() == element::value_type::object);

	using namespace bsoncxx;

	using builder::basic::kvp;

	builder::basic::document doc{};

	for (auto i = e.begin(); i != e.end(); ++i)
	{
		switch (i->type())
		{
			case detail::value_type::null:
				doc.append(kvp(i.key(), types::b_null{}));
				break;
			
			case detail::value_type::object:
				doc.append(kvp(i.key(), to_bson(i.value())));
				break;
			
			case detail::value_type::array:
			{
				builder::basic::array arr{};
				for (auto &a : i.value())
				{
					switch (a.type())
					{
						case detail::value_type::null:
							arr.append(types::b_null{});
							break;

						case detail::value_type::object:
							arr.append(to_bson(a));
							break;

						case detail::value_type::array:

							break;

						case detail::value_type::string:
							arr.append(a.as<std::string>());
							break;

						case detail::value_type::number_int:
							arr.append(a.as<int64_t>());
							break;

						case detail::value_type::number_float:
							arr.append(a.as<double>());
							break;

						case detail::value_type::boolean:
							arr.append(a.as<bool>());
							break;
					}
				}
				doc.append(kvp(i.key(), arr.extract()));
				break;
			}
			
			case detail::value_type::string:
				doc.append(kvp(i.key(), i->as<std::string>()));
				break;
			
			case detail::value_type::number_int:
				doc.append(kvp(i.key(), i->as<int64_t>()));
				break;
			
			case detail::value_type::number_float:
				doc.append(kvp(i.key(), i->as<double>()));
				break;
			
			case detail::value_type::boolean:
				doc.append(kvp(i.key(), i->as<bool>()));
				break;
		}
	}

	return doc.extract();
}


}