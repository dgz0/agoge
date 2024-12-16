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

#include <assert.h>

#include "agoge-compiler.h"
#include "agoge-sched.h"
#include "util.h"

static size_t parent_node_get(const size_t idx)
{
	return (idx - 1) / 2;
}

static size_t left_child_node_get(const size_t idx)
{
	return (idx * 2) + 1;
}

static size_t right_child_node_get(const size_t idx)
{
	return left_child_node_get(idx) + 1;
}

NONNULL size_t agoge_sched_ev_add(struct agoge_sched *const sched,
				  struct agoge_sched_ev *const ev)
{
	assert(sched->num_events < AGOGE_SCHED_NUM_EVENTS_MAX);

	ev->ts += sched->curr_ts;

	size_t idx = sched->num_events;
	sched->events[sched->num_events++] = *ev;

	while (idx && (sched->events[parent_node_get(idx)].ts >
		       sched->events[idx].ts)) {
		SWAP(sched->events[parent_node_get(idx)], sched->events[idx]);
		idx = parent_node_get(idx);
	}
	return sched->num_events;
}

NONNULL static void heapify(struct agoge_sched *const sched, size_t idx)
{
	for (;;) {
		const size_t left_child_node = left_child_node_get(idx);
		const size_t right_child_node = right_child_node_get(idx);
		size_t smallest = idx;

		if ((left_child_node < sched->num_events) &&
		    (sched->events[left_child_node].ts <
		     sched->events[idx].ts)) {
			smallest = left_child_node;
		}

		if ((right_child_node < sched->num_events) &&
		    (sched->events[right_child_node].ts <
		     sched->events[smallest].ts)) {
			smallest = right_child_node;
		}

		if (smallest != idx) {
			SWAP(sched->events[idx], sched->events[smallest]);
			idx = smallest;
		} else {
			break;
		}
	}
}

NONNULL void agoge_sched_ev_del(struct agoge_sched *const sched, size_t ev)
{
	sched->events[ev].ts = 0;

	while (ev &&
	       (sched->events[parent_node_get(ev)].ts > sched->events[ev].ts)) {
		SWAP(sched->events[parent_node_get(ev)], sched->events[ev]);
		ev = parent_node_get(ev);
	}

	sched->events[0] = sched->events[sched->num_events - 1];
	sched->num_events--;

	heapify(sched, 0);
}

NONNULL void agoge_sched_step(struct agoge_sched *const sched)
{
	sched->curr_ts += 4;

	while ((sched->num_events) && (sched->events[0].ts == sched->curr_ts)) {
		sched->events[0].cb(sched->events[0].udata);
		agoge_sched_ev_del(sched, 0);
	}
}
