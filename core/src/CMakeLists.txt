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

set(SRCS
        agoge-bus.c
        agoge-cart-mbc1.c
        agoge-cart.c
        agoge-cpu.c
        agoge-ctx.c
        agoge-disasm.c
        agoge-log.c
)

set(HDRS agoge-log.h)

set(PUBLIC_HDRS ../include/agoge/ctx.h ../include/agoge/log.h)

add_library(core STATIC ${SRCS} ${HDRS} ${PUBLIC_HDRS})

target_include_directories(core PUBLIC ../include)
target_link_libraries(core PRIVATE agoge_build_cfg_c)
