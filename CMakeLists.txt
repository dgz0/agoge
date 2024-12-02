# SPDX-License-Identifier: MIT
#
# Copyright 2024 dgz
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the “Software”), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
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

# The minimum CMake version required is tied to the latest CMake version in the
# repositories of the oldest currently supported Ubuntu LTS release. This is
# done for the following reasons:
#
# a) This is in line with our aim to support an environment that is no more than
#    5 years old.
#
# b) IDEs that ship their own version of CMake (e.g. JetBrains CLion) should be
#    guaranteed to work out of the box with this project.
cmake_minimum_required(VERSION 3.16.3 FATAL_ERROR)

project(agoge
	VERSION 1.0.0.0
	DESCRIPTION "Game Boy emulator"
	HOMEPAGE_URL "https://github.com/dgz0/agoge"
	LANGUAGES C CXX
)

# I cannot believe this isn't standard behavior by now, even factoring in the
# 5 year support target...
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR
		"In-tree builds are forbidden to avoid cluttering up the "
		"project tree; create a separate build directory and run CMake "
		"from there.")
endif()

# Generate a "compile_commands.json" file containing the exact compiler calls
# for all translation units of the project in machine-readable form. This is
# required to use features such as code completion and real-time diagnostics in
# some text editors like Visual Studio Code.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Treat all warnings as errors; only useful for CI pipelines.
option(AGOGE_WARNINGS_ARE_ERRORS "Treat all warnings as errors" OFF)

# Enable the AddressSanitizer (ASan) and Undefined Behavior Sanitizer (UBSan) if
# possible.
option(AGOGE_ENABLE_SANITIZERS "Enable ASan and UBSan if possible" OFF)

# Compile with the maximum possible optimisations specific to the build machine.
option(
	AGOGE_OPTIMIZE_FOR_ARCH
	"Build with architecture-specific optimizations"
	OFF
)

# The minimum compiler versions are bound in the same way as the CMake version;
# see the top of this file for more details.
set(CLANG_VER_MIN "10.0.0")
set(GCC_VER_MIN "9.4.0")

set(
	C_COMPILER_TOO_OLD_MSG
	"Your C compiler version is too old for this project; you need to use "
	"at least clang >=${CLANG_VER_MIN} or gcc >=${GCC_VER_MIN}."
)

# These compiler flags are supported by both clang and gcc for C targets. If you
# change this list, it is your responsibility to ensure that this statement
# still holds true.
set(COMPILER_FLAGS_C
	-Waggregate-return
	-Wall
	-Walloca
	-Wbad-function-cast
	-Wcast-align
	-Wcast-qual
	-Wconversion
	-Wdate-time
	-Wdisabled-optimization
	-Wextra
	-Wfloat-equal
	-Wformat=2
	-Winit-self
	-Winline
	-Winvalid-pch
	-Wlong-long
	-Wmain
	-Wmissing-declarations
	-Wmissing-include-dirs
	-Wmissing-prototypes
	-Wnested-externs
	-Wnull-dereference
	-Wold-style-definition
	-Wpacked
	-Wpadded
	-Wredundant-decls
	-Wshadow
	-Wstrict-prototypes
	-Wswitch-default
	-Wswitch-enum
	-Wundef
	-Wuninitialized
	-Wunused-macros
	-Wvla
	-Wwrite-strings
)

# There are many variants of Clang that are supported by CMake, such as IBMClang
# or TIClang or FujitsuClang. This project does not support them, with the sole
# exception of AppleClang.
if ((CMAKE_C_COMPILER_ID STREQUAL "AppleClang") OR
    (CMAKE_C_COMPILER_ID STREQUAL "Clang"))
	if (CMAKE_C_COMPILER_VERSION LESS ${CLANG_VER_MIN})
		message(FATAL_ERROR ${C_COMPILER_TOO_OLD_MSG})
	endif()

	list(APPEND COMPILER_FLAGS_C "-glldb")
elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
	if (CMAKE_C_COMPILER_VERSION LESS ${GCC_VER_MIN})
		message(FATAL_ERROR ${C_COMPILER_TOO_OLD_MSG})
	endif()

	list(
		APPEND COMPILER_FLAGS_C
		-Walloc-zero
		-Wduplicated-branches
		-Wduplicated-cond
		-Wformat-overflow=2
		-Wformat-signedness
		-Wformat-truncation=2
		-Wjump-misses-init
		-Wlogical-op
		-Wmissing-profile
		-Wno-aggressive-loop-optimizations
		-Wstringop-truncation
		-Wsuggest-attribute=cold
		-Wsuggest-attribute=const
		-Wsuggest-attribute=format
		-Wsuggest-attribute=malloc
		-Wsuggest-attribute=noreturn
		-Wsuggest-attribute=pure
		-Wtrampolines
		-Wunsafe-loop-optimizations
		-Wunused-const-variable=2
		-ggdb3
	)
else()
	message(FATAL_ERROR
		"Your C compiler is not supported by this project; you need to "
		"use clang >=${CLANG_VER_MIN} or gcc >=${GCC_VER_MIN}.")
endif()

if (AGOGE_OPTIMIZE_FOR_ARCH)
	list(APPEND COMPILER_FLAGS_C -march=native -mtune=native)
endif()

if (AGOGE_WARNINGS_ARE_ERRORS)
	list(APPEND COMPILER_FLAGS_C -Werror)
endif()

if (AGOGE_ENABLE_SANITIZERS)
	list(APPEND COMPILER_FLAGS_C -fsanitize=address,undefined)
endif()

# An INTERFACE library target does not directly create build output, though it
# may have properties set on it and it may be installed, exported and imported.
# In this case, when targets link to this library, they will inherit the project
# wide settings that we set here.
add_library(agoge_build_cfg_c INTERFACE)
target_compile_options(agoge_build_cfg_c INTERFACE ${COMPILER_FLAGS_C})

add_subdirectory(core)
add_subdirectory(app)
