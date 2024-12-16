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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stddef.h>
#include <stdint.h>

/// The maximum number of events we can handle.
#define AGOGE_SCHED_NUM_EVENTS_MAX (32)

enum agoge_sched_event_group {
	AGOGE_SCHED_EVENT_GROUP_NONE = 0,
	AGOGE_SCHED_EVENT_GROUP_TIMER = 1
};

struct agoge_sched_ev {
	enum agoge_sched_event_group group;
	uintmax_t ts;
	void (*cb)(void *ptr);
	void *udata;
};

struct agoge_sched {
	uintmax_t curr_ts;
	struct agoge_sched_ev events[AGOGE_SCHED_NUM_EVENTS_MAX];
	size_t num_events;
};

#ifdef __cplusplus
}
#endif // __cplusplus
