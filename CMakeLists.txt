# SPDX-License-Identifier: BSD-2-Clause

# Copyright (c) 2021 NKI/AVL, Netherlands Cancer Institute

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:

# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required(VERSION 3.21)

# set the project name
project(pdb-redo-archive-manager VERSION 1.0.1 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CMakePackageConfigHelpers)
include(Dart)
include(FindFilesystem)
include(FindPkgConfig)
include(GenerateExportHeader)

set(CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Filesystem REQUIRED)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif()

if(MSVC)
    # make msvc standards compliant...
    add_compile_options(/permissive-)

	# This is at least Windows 10, right?
	add_definitions(-D _WIN32_WINNT=0x0A00)
endif()

message(STATUS "The program will be installed in ${CMAKE_INSTALL_FULL_SBINDIR}")

# Create a revision file, containing the current git version info, if any
include(VersionString)
write_version_header("${CMAKE_CURRENT_SOURCE_DIR}/src")

# Optionally use mrc to create resources
find_package(Mrc QUIET)

if(MRC_FOUND)
	option(USE_RSRC "Use mrc to create resources" ON)
else()
	message(STATUS "Not using resources since mrc was not found")
endif()

if(USE_RSRC)
	set(USE_RSRC 1)
	add_compile_definitions(USE_RSRC WEBAPP_USES_RESOURCES)

	mrc_write_header(${CMAKE_BINARY_DIR}/mrsrc.hpp)
endif()

# libraries

set(CMAKE_THREAD_PREFER_PTHREAD)
set(THREADS_PREFER_PTHREAD_FLAG)
find_package(Threads)

include(FindFilesystem)
list(APPEND REQUIRED_LIBRARIES ${STDCPPFS_LIBRARY})

include(FindAtomic)
list(APPEND REQUIRED_LIBRARIES ${STDCPPATOMIC_LIBRARY})

find_package(LibArchive REQUIRED)

find_program(YARN yarn REQUIRED)

find_package(zeep 5.1.4 REQUIRED)
find_package(gxrio REQUIRED)
find_package(libpqxx 7.8 REQUIRED)

pkg_check_modules(PQ REQUIRED IMPORTED_TARGET libpq)

# As long as C++20 is not fully implemented (and used)
find_package(date REQUIRED)

add_executable(pramd
	${PROJECT_SOURCE_DIR}/src/pramd.cpp
	${PROJECT_SOURCE_DIR}/src/data-service.cpp
	${PROJECT_SOURCE_DIR}/src/db-connection.cpp
	${PROJECT_SOURCE_DIR}/src/utilities.cpp)

target_compile_definitions(pramd
	PRIVATE
		$<$<CONFIG:Release>:NDEBUG>
		$<$<CONFIG:Debug>:DEBUG>
)

# yarn rules for javascripts
	
set(webpack_input
	${PROJECT_SOURCE_DIR}/webapp/index.js
	${PROJECT_SOURCE_DIR}/webapp/lists.js
	${PROJECT_SOURCE_DIR}/scss/pdb-redo-bootstrap.scss
	${PROJECT_SOURCE_DIR}/webapp/search.js)

set(webpack_output ${PROJECT_SOURCE_DIR}/docroot/scripts/index.js)

add_custom_command(
	OUTPUT ${webpack_output}
	BYPRODUCTS 
	DEPENDS ${webpack_input}
	COMMAND ${YARN} run $<IF:$<CONFIG:Debug>,build,build-production>
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_custom_target(webpack DEPENDS ${webpack_output})

add_dependencies(pramd webpack)

if(USE_RSRC)
	mrc_target_resources(pramd
		${PROJECT_SOURCE_DIR}/docroot/
		${PROJECT_SOURCE_DIR}/rsrc/db-schema.sql
		${PROJECT_SOURCE_DIR}/rsrc/data.json.schema
	)
endif()

target_include_directories(pramd PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR} ${PQ_INCLUDE_DIRS})
target_link_libraries(pramd	-static-libgcc -static-libstdc++
	date::date zeep::zeep std::filesystem LibArchive::LibArchive ${REQUIRED_LIBRARIES} gxrio::gxrio PkgConfig::PQ libpqxx::pqxx)

install(TARGETS pramd
    RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)

# # manual

# install(FILES doc/pramd.1 DESTINATION ${CMAKE_INSTALL_DATADIR}/man/man1)

# manual

# install(FILES doc/pramd.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
