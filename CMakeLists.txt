# Copyright 2025 dgz
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the “Software”), to deal
# in the Software without restriction, including without limitation the rights
# to use copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Call the cmake_minimum_required() command at the beginning of the top-level
# CMakeLists.txt file even before calling the project() command. It is important
# to establish version and policy settings before invoking other commands whose
# behavior they may affect. See also policy CMP0000.
#
# The FATAL_ERROR option is accepted but ignored by CMake 2.6 and higher. It
# should be specified so CMake versions 2.4 and lower fail with an error instead
# of just a warning.
cmake_minimum_required(VERSION 3.22.1 FATAL_ERROR)

project(
	agoge
	VERSION 0.1.0
	DESCRIPTION "Game Boy emulator"
	HOMEPAGE_URL "https://github.com/dgz0/agoge"
	LANGUAGES C)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR
		"In-tree builds are not supported. Please create a separate "
		"build directory and run CMake from there.")
endif()

if (CMAKE_VERSION LESS "3.26")
	# CMake versions >=3.26 set this property to ON by default.
	set(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

# Generate a compile_commands.json file containing the exact compiler calls for
# all translation units of the project in machine-readable form.
#
# This option is implemented only by Makefile Generators and Ninja Generators.
# It is ignored on other generators.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(VER_MIN_CLANG "14.0.0")
set(VER_MIN_GCC "11.4.0")

option(AGOGE_ENABLE_SANITIZERS "Enable ASan and UBSan" OFF)

function(agoge_base_c_init)
	# These flags are supported by both clang and gcc for C targets only.
	set(C_FLAGS_LIST
		-Wall
		-Walloc-zero
		-Walloca
		-Warray-bounds=2
		-Wattribute-alias=2
		-Wbad-function-cast
		-Wcast-align
		-Wcast-qual
		-Wdate-time
		-Wdisabled-optimization
		-Wdouble-promotion
		-Wextra
		-Wfloat-equal
		-Wformat-overflow=2
		-Wformat-signedness
		-Wformat-truncation=2
		-Wformat=2
		-Winit-self
		-Winline
		-Winvalid-pch
		-Wjump-misses-init
		-Wmain
		-Wmissing-declarations
		-Wmissing-include-dirs
		-Wmissing-prototypes
		-Wnested-externs
		-Wno-aggressive-loop-optimizations
		-Wno-coverage-mismatch
		-Wold-style-declaration
		-Wold-style-definition
		-Wpacked
		-Wredundant-decls
		-Wshadow
		-Wshift-overflow=2
		-Wstrict-overflow=4
		-Wstrict-prototypes
		-Wstringop-overflow=4
		-Wsuggest-attribute=cold
		-Wsuggest-attribute=const
		-Wsuggest-attribute=format
		-Wsuggest-attribute=malloc
		-Wsuggest-attribute=noreturn
		-Wsuggest-attribute=pure
		-Wswitch-default
		-Wswitch-enum
		-Wtrampolines
		-Wundef
		-Wuninitialized
		-Wunsafe-loop-optimizations
		-Wunsuffixed-float-constants
		-Wunused-const-variable=2
		-Wunused-macros
		-Wvector-operation-performance
		-Wvla
		-Wwrite-strings
		-fdiagnostics-color=always
	)

	set(C_COMPILER_INVALID_MSG
		"Your C compiler version is too old for this project. Please "
		"use clang >=${VER_MIN_CLANG} or gcc >=${VER_MIN_GCC}."
	)

	if ((CMAKE_C_COMPILER_ID STREQUAL "AppleClang") OR
	    (CMAKE_C_COMPILER_ID STREQUAL "Clang"))
		if (CMAKE_C_COMPILER_VERSION LESS ${VER_MIN_CLANG})
			message(FATAL_ERROR ${C_COMPILER_INVALID_MSG})
		endif()

		list(APPEND C_FLAGS_LIST "-glldb")
	elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
		if (CMAKE_C_COMPILER_VERSION LESS ${VER_MIN_GCC})
			message(FATAL_ERROR ${C_COMPILER_INVALID_MSG})
		endif()

		list(APPEND C_FLAGS_LIST
			-Walloc-zero
			-ggdb3
		)
	else()
		message(FATAL_ERROR
			"Your C compiler is not supported by agoge; please use "
			"clang >=${VER_MIN_CLANG} or gcc >=${VER_MIN_GCC}."
		)
	endif()

	if (AGOGE_OPTIMIZE_FOR_ARCH)
		list(APPEND C_FLAGS_LIST -march=native -mtune=native)
	endif()

	if (AGOGE_WARNINGS_ARE_ERRORS)
		list(APPEND C_FLAGS_LIST -Werror)
	endif()

	add_library(agoge_base_c INTERFACE)

	if (AGOGE_ENABLE_SANITIZERS)
		list(APPEND C_FLAGS_LIST -fsanitize=address,undefined)

		target_link_options(agoge_base_c INTERFACE
			-fsanitize=address,undefined
		)
	endif()
	
	target_compile_options(agoge_base_c INTERFACE ${C_FLAGS_LIST})
endfunction()

agoge_base_c_init()

add_subdirectory(core)
add_subdirectory(app)
