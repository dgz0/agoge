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

set(SRCS bus.c cart.c cpu.c ctx.c disasm.c log.c)
set(HDRS bus.h cart.h cpu.h disasm.h log.h)

set(HDRS_PUBLIC
        ../include/agoge/bus.h
        ../include/agoge/cart.h
        ../include/agoge/cpu.h
        ../include/agoge/ctx.h
        ../include/agoge/disasm.h
        ../include/agoge/log.h
)

add_library(agoge STATIC ${SRCS} ${HDRS} ${HDRS_PUBLIC})

target_link_libraries(agoge PRIVATE agoge_base_c)

target_include_directories(
        agoge PUBLIC ../include
)
