cmake_minimum_required(VERSION 3.16)

project(WindTermHubTerm LANGUAGES CXX)

option(WINDTERM_BUILD_HUBTERM_ONLY "Build only the HubTerm integration module from this partial WindTerm source tree" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT WINDTERM_BUILD_HUBTERM_ONLY)
	message(FATAL_ERROR "This repository snapshot only provides a standalone HubTerm module build target.")
endif()

add_subdirectory(src/HubTerm)
