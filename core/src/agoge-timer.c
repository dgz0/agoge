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

#include "agoge-compiler.h"
#include "agoge-sched.h"
#include "agoge-timer.h"

#define TAC_BIT_ENABLE (UINT8_C(1) << 2)
#define TAC_MASK_CLKSEL (UINT8_C(0b00000011))

#define TAC_CLKSEL_M_CYCLE_256 (0b00)
#define TAC_CLKSEL_M_CYCLE_4 (0b01)
#define TAC_CLKSEL_M_CYCLE_16 (0b10)
#define TAC_CLKSEL_M_CYCLE_64 (0b11)

#define IF_TIMER (UINT8_C(1) << 2)

static const unsigned int m_cycles[] = { [TAC_CLKSEL_M_CYCLE_256] = 1024,
					 [TAC_CLKSEL_M_CYCLE_4] = 16,
					 [TAC_CLKSEL_M_CYCLE_16] = 64,
					 [TAC_CLKSEL_M_CYCLE_64] = 256 };

NONNULL static void tima_inc(void *const udata)
{
	struct agoge_timer *timer = (struct agoge_timer *)udata;
	timer->tima++;

	struct agoge_sched_ev ev = {
		.group = AGOGE_SCHED_EVENT_GROUP_TIMER,
		.ts = m_cycles[timer->tac & TAC_MASK_CLKSEL],
		.cb = &tima_inc,
		.udata = timer
	};
	agoge_sched_ev_add(timer->sched, &ev);
}

NONNULL static void tima_ovf_stage2(void *const udata)
{
	struct agoge_timer *timer = (struct agoge_timer *)udata;

	timer->tima = timer->tma;
	*timer->intr_flag |= IF_TIMER;
}

NONNULL static void tima_ovf_stage1(void *const udata)
{
	struct agoge_timer *timer = (struct agoge_timer *)udata;
	timer->tima = 0x00;
}

NONNULL static void tima_inc_ev_add(struct agoge_timer *const timer,
				    const uint8_t tac)
{
	struct agoge_sched_ev ev = { .group = AGOGE_SCHED_EVENT_GROUP_TIMER,
				     .ts = m_cycles[tac & TAC_MASK_CLKSEL],
				     .cb = &tima_inc,
				     .udata = timer };

	agoge_sched_ev_add(timer->sched, &ev);
}

NONNULL static void tima_ovf_ev_add(struct agoge_timer *const timer)
{
	const uintmax_t ts = (UINT8_MAX - timer->tima) *
			     m_cycles[timer->tac & TAC_MASK_CLKSEL];

	struct agoge_sched_ev ev_tima_ovf_stage1 = {
		.group = AGOGE_SCHED_EVENT_GROUP_TIMER,
		.ts = ts,
		.cb = &tima_ovf_stage1,
		.udata = timer
	};

	struct agoge_sched_ev ev_tima_ovf_stage2 = {
		.group = AGOGE_SCHED_EVENT_GROUP_TIMER,
		.ts = ts + 4,
		.cb = &tima_ovf_stage2,
		.udata = timer
	};

	timer->ev_tima_ovf_stage1 =
		agoge_sched_ev_add(timer->sched, &ev_tima_ovf_stage1);

	timer->ev_tima_ovf_stage2 =
		agoge_sched_ev_add(timer->sched, &ev_tima_ovf_stage2);
}

NONNULL void agoge_timer_write_tima(struct agoge_timer *const timer,
				    const uint8_t tima)
{
	timer->tima = tima;

	if (timer->tac & TAC_BIT_ENABLE) {
		agoge_sched_ev_del(timer->sched, timer->ev_tima_ovf_stage1);
		agoge_sched_ev_del(timer->sched, timer->ev_tima_ovf_stage2);

		tima_ovf_ev_add(timer);
	}
}

NONNULL void agoge_timer_write_tac(struct agoge_timer *const timer,
				   const uint8_t tac)
{
	if (likely(!(timer->tac & TAC_BIT_ENABLE)) && (tac & TAC_BIT_ENABLE)) {
		tima_inc_ev_add(timer, tac);
		tima_ovf_ev_add(timer);
		timer->tac = tac;
	}
}
