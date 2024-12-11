// SPDX-License-Identifier: MIT
//
// Copyright 2024 dgz
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the “Software”), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/// @file agoge-compiler.h Defines abstractions around various compiler features
/// for easier readability.

#pragma once

/// All the parameters in a function are expected to be non-NULL.
#define NONNULL __attribute__((nonnull))

/// Control flow is unreachable at a location.
#define UNREACHABLE __builtin_unreachable()

/// The return value of a function must not be discarded.
#define NODISCARD __attribute__((warn_unused_result))

/// This function is a printf-like function, and the format string passed to it
/// is checked at compile-time for validity.
///
/// @param string_index Specifies which argument is the format string argument
/// (starting from 1).
///
/// @param first_to_check The number of the first argument to check against the
/// format string.
#define FORMAT_PRINTF(string_index, first_to_check) \
	__attribute__((format(printf, string_index, first_to_check)))

/// This function will always be inlined regardless of compiler heuristics or
/// optimization level.
#define ALWAYS_INLINE inline __attribute__((always_inline))

/// This branch is unlikely to be executed.
#define unlikely(x) __builtin_expect(!!(x), 0)
