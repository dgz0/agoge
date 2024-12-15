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

NONNULL void agoge_sched_event_add(struct agoge_sched *const sched,
				   struct agoge_sched_event *const event)
{
	assert(sched->num_events < AGOGE_SCHED_NUM_EVENTS_MAX);

	event->ts += sched->curr_ts;

	size_t idx = sched->num_events;
	sched->events[sched->num_events++] = *event;

	while (idx && (sched->events[parent_node_get(idx)].ts >
		       sched->events[idx].ts)) {
		SWAP(sched->events[parent_node_get(idx)], sched->events[idx]);
		idx = parent_node_get(idx);
	}
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

NONNULL static void delete_element(struct agoge_sched *const sched, size_t idx)
{
	sched->events[idx].ts = 0;

	while (idx && (sched->events[parent_node_get(idx)].ts >
		       sched->events[idx].ts)) {
		SWAP(sched->events[parent_node_get(idx)], sched->events[idx]);
		idx = parent_node_get(idx);
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
		delete_element(sched, 0);
	}
}
