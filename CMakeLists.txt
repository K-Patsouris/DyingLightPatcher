cmake_minimum_required(VERSION 3.10)

project(
	DLPatcher
	VERSION 1.0.0
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_DIR "${PROJECT_SOURCE_DIR}/src")
set(SOURCE_FILES
	"${SOURCE_DIR}/AssosciativeCache.h"
	"${SOURCE_DIR}/Common.h"
	"${SOURCE_DIR}/ConsoleHandler.cpp"
	"${SOURCE_DIR}/ConsoleHandler.h"
	"${SOURCE_DIR}/Containers.cpp"
	"${SOURCE_DIR}/Containers.h"
	"${SOURCE_DIR}/CTestTestfile.cmake"
	"${SOURCE_DIR}/FileManager.cpp"
	"${SOURCE_DIR}/FileManager.h"
	"${SOURCE_DIR}/main.cpp"
	"${SOURCE_DIR}/Logger.cpp"
	"${SOURCE_DIR}/Logger.h"
	"${SOURCE_DIR}/StringParser.cpp"
	"${SOURCE_DIR}/StringParser.h"
	"${SOURCE_DIR}/Types.h"
	"${SOURCE_DIR}/Utils.cpp"
	"${SOURCE_DIR}/Utils.h"
)

add_executable(DLPatcher ${SOURCE_FILES})

target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_20)

find_package(libzippp CONFIG REQUIRED)
target_link_libraries(DLPatcher PRIVATE libzippp::libzippp)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	target_compile_options(
		"${PROJECT_NAME}"
		PRIVATE
			"/std:c17"	# Force some relevant C for potential .c files
			
			"/sdl"	# Enable Additional Security Checks
			"/utf-8"	# Set Source and Executable character sets to UTF-8
			"/Zi"	# Debug Information Format

			"/permissive-"	# Standards conformance
			"/Zc:preprocessor"	# Enable preprocessor conformance mode

			"/wd4200" # nonstandard extension used : zero-sized array in struct/union

			"$<$<CONFIG:DEBUG>:>"
			"$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/O2;/Ob2>;/Oi;/Ot;/GL"
	)

	target_link_options(
		"${PROJECT_NAME}"
		PRIVATE
			"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
			"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>;/LTCG:incremental"
	)
endif()


