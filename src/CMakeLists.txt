#
# Copyright (c) 2019 -2021 MINRES Technolgies GmbH
#
# SPDX-License-Identifier: Apache-2.0
#
cmake_minimum_required(VERSION 3.12)

project(tgc-vp LANGUAGES C CXX VERSION 0.0.1)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/etiss-sc/cmake/)

option(USE_ETISS "Use ETISS" OFF)

set(SRCS
	sc_main.cpp
	CLIParser.cpp
	tgc_vp/tb.cpp
	tgc_vp/system.cpp
)
set(INCLUDES
	${CMAKE_CURRENT_LIST_DIR}
)
set(LIBS
	dbt-rise-tgc_sc
	vpvper_generic
	vpvper_sifive
)

if (USE_ETISS)
	list(APPEND SRCS
		"${CMAKE_SOURCE_DIR}/etiss-sc/src/utils/config.cpp"
		"${CMAKE_SOURCE_DIR}/etiss-sc/src/utils/plugins.cpp"
		"${CMAKE_SOURCE_DIR}/etiss-sc/src/utils/logging.cpp"
		"${CMAKE_SOURCE_DIR}/etiss-sc/src/tlm/generic/cpu_minres.cpp"
	)

	list(APPEND INCLUDES
		${CMAKE_SOURCE_DIR}/etiss-sc/include/
	)

	list(APPEND LIBS
		ETISS::etiss
	)
endif()

find_package(Boost COMPONENTS program_options thread REQUIRED)
if (USE_ETISS)
	find_package(ETISS REQUIRED)
endif()

add_executable(${PROJECT_NAME} ${SRCS})

target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBS} ${BOOST_program_options_LIBRARY})

if(TARGET Boost::program_options)
	target_link_libraries(${PROJECT_NAME} PUBLIC Boost::program_options Boost::thread)
else()
	target_link_libraries(${PROJECT_NAME} PUBLIC ${BOOST_program_options_LIBRARY})
endif()

if (USE_ETISS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ETISS)
endif()