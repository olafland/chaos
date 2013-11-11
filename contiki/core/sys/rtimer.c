/**
 * \addtogroup rt
 * @{
 */

/**
 * \file
 *         Implementation of the architecture-agnostic parts of the real-time timer module.
 * \author
 *         Adam Dunkels <adam@sics.se>
 *
 */


/*
 * Copyright (c) 2005, Swedish Institute of Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * @(#)$Id: rtimer.c,v 1.7 2010/01/19 13:08:24 adamdunkels Exp $
 */

#include "sys/rtimer.h"
#include "contiki.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

static struct rtimer *next_rtimer;

/*---------------------------------------------------------------------------*/
void
rtimer_init(void)
{
  rtimer_arch_init();
}
/*---------------------------------------------------------------------------*/
int
rtimer_set(struct rtimer *rtimer, rtimer_clock_t time,
	   rtimer_clock_t duration,
	   rtimer_callback_t func, void *ptr)
{
  int first = 0;

  PRINTF("rtimer_set time %d\n", time);

  if(next_rtimer == NULL) {
    first = 1;
  }

  rtimer->func = func;
  rtimer->ptr = ptr;
  rtimer->overflows_to_go = 0;

  rtimer->time = time;
  next_rtimer = rtimer;

  if(first == 1) {
    rtimer_arch_schedule(time);
  }
  return RTIMER_OK;
}
/*---------------------------------------------------------------------------*/
int
rtimer_set_long(struct rtimer *rtimer, rtimer_clock_t ref_time, unsigned long offset,
	   rtimer_callback_t func, void *ptr)
{
  if(next_rtimer == NULL) {
    rtimer->func = func;
    rtimer->ptr = ptr;
    rtimer->time = ref_time + offset;
    next_rtimer = rtimer;

    // it is assumed here that the timer is scheduled within 2 seconds after ref_time
    if (offset < (unsigned long)RTIMER_SECOND * 2) {
    	rtimer->overflows_to_go = 0;
    } else {
        rtimer_clock_t now = RTIMER_NOW();
        rtimer->overflows_to_go = (offset - (now - ref_time)) >> 16;
        // It should never happen, but to be sure...
        if (rtimer->overflows_to_go == 0xffff) {
        	rtimer->overflows_to_go = 0;
        }
    }
	rtimer_arch_schedule(ref_time + (rtimer_clock_t)offset);

//    printf("now %u, ref_time %u, offset %lu, TACCR0 %u\n",
//     		now, ref_time, offset, TACCR0);
  }
  return RTIMER_OK;
}
/*---------------------------------------------------------------------------*/
unsigned long
rtimer_time_to_expire(void) {
	return (unsigned long)(TACCR0 - RTIMER_NOW()) +
			((unsigned long)next_rtimer->overflows_to_go << 16);
}
/*---------------------------------------------------------------------------*/
void
rtimer_run_next(void)
{
  struct rtimer *t;
  if(next_rtimer == NULL) {
    return;
  }
  t = next_rtimer;
  next_rtimer = NULL;
  if (t->overflows_to_go == 0) {
	  // no more overflows to wait for
	  t->func(t, t->ptr);
  } else {
	  // we still have to wait for more timer overflows
	  t->overflows_to_go--;
	  next_rtimer = t;
  }
  if(next_rtimer != NULL) {
    rtimer_arch_schedule(next_rtimer->time);
  }
  return;
}
/*---------------------------------------------------------------------------*/
