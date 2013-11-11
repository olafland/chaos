/*
 * Copyright (c) 2011, ETH Zurich.
 * Copyright (c) 2013, Olaf Landsiedel.
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
 * Author: Federico Ferrari <ferrari@tik.ee.ethz.ch>
 * Author: Olaf Landsiedel <olafl@chalmers.se>
 *
 */

/**
 * \file
 *         Chaos core, header file.
 * \author
 *         Olaf Landsiedel <olafl@chalmers.se>
 * \author
 *         Federico Ferrari <ferrari@tik.ee.ethz.ch>
 */

#ifndef CHAOS_H_
#define CHAOS_H_

#include "contiki.h"
#include "dev/watchdog.h"
#include "dev/cc2420_const.h"
#include "dev/leds.h"
#include "dev/spi.h"
#include <stdio.h>
#include <legacymsp430.h>
#include <stdlib.h>
#include "lib/random.h"

/**
 * Number of clock (DCO) cycles reserved for flags and payload processing.
 */
#ifndef PROCESSING_CYCLES
#define PROCESSING_CYCLES            40000
#endif

/**
 * If not zero, nodes print additional debug information (disabled by default).
 */
#define CHAOS_DEBUG 1
// #define LOG_TIRQ 1
#define LOG_FLAGS 1
//#define LOG_ALL_FLAGS 1

/**
 * Size of the window used to average estimations of slot lengths.
 */
#define CHAOS_SYNC_WINDOW             32

#ifndef FINAL_CHAOS_FLOOD
#define FINAL_CHAOS_FLOOD              1 // are the final Chaos floods enabled?
#endif

#ifndef TIMEOUT
#define TIMEOUT                         1 // are timeouts enabled?
#endif

#ifndef MIN_SLOTS_TIMEOUT
#define MIN_SLOTS_TIMEOUT               3 // minimum slots with no rx before timeout expire, never put below two
#endif

#ifndef MAX_SLOTS_TIMEOUT
#define MAX_SLOTS_TIMEOUT               7 // maximum slots with no rx before timeout expires
#endif

#ifndef CC2420_TXPOWER
#define CC2420_TXPOWER CC2420_TXPOWER_MAX
#endif

#define BYTES_TIMEOUT                  32

/**
 * Ratio between the frequencies of the DCO and the low-frequency clocks
 */
#if COOJA
#define CLOCK_PHI                     (4194304uL / RTIMER_SECOND)
#else
#define CLOCK_PHI                     (F_CPU / RTIMER_SECOND)
#endif /* COOJA */

#define CHAOS_HEADER                 0xfe
#define CHAOS_HEADER_LEN             sizeof(uint8_t)
#define CHAOS_RELAY_CNT_LEN          sizeof(uint8_t)
#define CHAOS_IS_ON()                (get_state() != CHAOS_STATE_OFF)
#define FOOTER_LEN                    2
#define FOOTER1_CRC_OK                0x80
#define FOOTER1_CORRELATION           0x7f


#if CHAOS_SYNC_MODE == CHAOS_SYNC
#define PACKET_LEN (DATA_LEN + FOOTER_LEN + CHAOS_RELAY_CNT_LEN + CHAOS_HEADER_LEN)
#else
#define PACKET_LEN (DATA_LEN + FOOTER_LEN + CHAOS_HEADER_LEN)
#endif

#define CHAOS_LEN_FIELD              packet[0]
#define CHAOS_HEADER_FIELD           packet[1]
#define CHAOS_DATA_FIELD             packet[2]
#define CHAOS_BYTES_TIMEOUT_FIELD    packet[2+BYTES_TIMEOUT]
#define CHAOS_RELAY_CNT_FIELD        packet[PACKET_LEN - FOOTER_LEN]
#define CHAOS_RSSI_FIELD             packet[PACKET_LEN - 1]
#define CHAOS_CRC_FIELD              packet[PACKET_LEN]

enum {
	CHAOS_INITIATOR = 1, CHAOS_RECEIVER = 0
};

enum {
	CHAOS_SYNC = 1, CHAOS_NO_SYNC = 0
};

enum {
	CHAOS_COMPLETE = 1, CHAOS_INCOMPLETE = 0
};

/**
 * List of possible Chaos states.
 */
enum chaos_state {
	CHAOS_STATE_OFF,          /**< Chaos is not executing */
	CHAOS_STATE_WAITING,      /**< Chaos is waiting for a packet being flooded */
	CHAOS_STATE_RECEIVING,    /**< Chaos is receiving a packet */
	CHAOS_STATE_RECEIVED,     /**< Chaos has just finished receiving a packet */
	CHAOS_STATE_TRANSMITTING, /**< Chaos is transmitting a packet */
	CHAOS_STATE_TRANSMITTED,  /**< Chaos has just finished transmitting a packet */
	CHAOS_STATE_ABORTED       /**< Chaos has just aborted a packet reception */
};
#if CHAOS_DEBUG
unsigned int high_T_irq, rx_timeout, bad_length, bad_header, bad_crc, rc_update;
#endif /* CHAOS_DEBUG */

PROCESS_NAME(chaos_process);

/* ----------------------- Application interface -------------------- */
/**
 * \defgroup chaos_interface Chaos API
 * @{
 * \file   chaos.h
 * \file   chaos.c
 */

/**
 * \defgroup chaos_main Interface related to flooding
 * @{
 */

/**
 * \brief            Start Chaos and stall all other application tasks.
 *
 * \param data_      A pointer to the flooding data.
 *
 *                   At the initiator, Chaos reads from the given memory
 *                   location data provided by the application.
 *
 *                   At a receiver, Chaos writes to the given memory
 *                   location data for the application.
 * \param data_len_  Length of the flooding data, in bytes.
 * \param initiator_ Not zero if the node is the initiator,
 *                   zero if it is a receiver.
 * \param sync_      Not zero if Chaos must provide time synchronization,
 *                   zero otherwise.
 * \param tx_max_    Maximum number of transmissions (N).
 */
void chaos_start(uint8_t *data_, /*uint8_t data_len_,*/ uint8_t initiator_,
		/*uint8_t sync_,*/ uint8_t tx_max_);

/**
 * \brief            Stop Chaos and resume all other application tasks.
 * \returns          Number of times the packet has been received during
 *                   last Chaos phase.
 *                   If it is zero, the packet was not successfully received.
 * \sa               get_rx_cnt
 */
uint8_t chaos_stop(void);

/**
 * \brief            Get the last received counter.
 * \returns          Number of times the packet has been received during
 *                   last Chaos phase.
 *                   If it is zero, the packet was not successfully received.
 */
uint8_t get_rx_cnt(void);

/**
 * \brief            Get the current Chaos state.
 * \return           Current Chaos state, one of the possible values
 *                   of \link chaos_state \endlink.
 */
uint8_t get_state(void);

/**
 * \brief            Get low-frequency time of first packet reception
 *                   during the last Chaos phase.
 * \returns          Low-frequency time of first packet reception
 *                   during the last Chaos phase.
 */
rtimer_clock_t get_t_first_rx_l(void);

/** @} */

/**
 * \defgroup chaos_sync Interface related to time synchronization
 * @{
 */

/**
 * \brief            Get the last relay counter.
 * \returns          Value of the relay counter embedded in the first packet
 *                   received during the last Chaos phase.
 */
uint8_t get_relay_cnt(void);

/**
 * \brief            Get the local estimation of T_slot, in DCO clock ticks.
 * \returns          Local estimation of T_slot.
 */
rtimer_clock_t get_T_slot_h(void);

/**
 * \brief            Get low-frequency synchronization reference time.
 * \returns          Low-frequency reference time
 *                   (i.e., time at which the initiator started the flood).
 */
rtimer_clock_t get_t_ref_l(void);

/**
 * \brief            Provide information about current synchronization status.
 * \returns          Not zero if the synchronization reference time was
 *                   updated during the last Chaos phase, zero otherwise.
 */
uint8_t is_t_ref_l_updated(void);

/**
 * \brief            Set low-frequency synchronization reference time.
 * \param t          Updated reference time.
 *                   Useful to manually update the reference time if a
 *                   packet has not been received.
 */
void set_t_ref_l(rtimer_clock_t t);

/**
 * \brief            Set the current synchronization status.
 * \param updated    Not zero if a node has to be considered synchronized,
 *                   zero otherwise.
 */
void set_t_ref_l_updated(uint8_t updated);

/** @} */

/** @} */

/**
 * \defgroup chaos_internal Chaos internal functions
 * @{
 * \file   chaos.h
 * \file   chaos.c
 */

/* ------------------------------ Timeouts -------------------------- */
/**
 * \defgroup chaos_timeouts Timeouts
 * @{
 */

inline void chaos_schedule_rx_timeout(void);
inline void chaos_stop_rx_timeout(void);

/** @} */

/* ----------------------- Interrupt functions ---------------------- */
/**
 * \defgroup chaos_interrupts Interrupt functions
 * @{
 */

inline void chaos_begin_rx(void);
inline void chaos_end_rx(void);
inline void chaos_begin_tx(void);
inline void chaos_end_tx(void);

inline void print_tirq(void);
inline void print_flags_tx(void);
inline void print_flags_rx(void);

/** @} */

/**
 * \defgroup chaos_capture Timer capture of clock ticks
 * @{
 */

/* -------------------------- Clock Capture ------------------------- */
/**
 * \brief Capture next low-frequency clock tick and DCO clock value at that instant.
 * \param t_cap_h variable for storing value of DCO clock value
 * \param t_cap_l variable for storing value of low-frequency clock value
 */
#define CAPTURE_NEXT_CLOCK_TICK(t_cap_h, t_cap_l) do {\
		/* Enable capture mode for timers B6 and A2 (ACLK) */\
		TBCCTL6 = CCIS0 | CM_POS | CAP | SCS; \
		TACCTL2 = CCIS0 | CM_POS | CAP | SCS; \
		/* Wait until both timers capture the next clock tick */\
		while (!((TBCCTL6 & CCIFG) && (TACCTL2 & CCIFG))); \
		/* Store the capture timer values */\
		t_cap_h = TBCCR6; \
		t_cap_l = TACCR2; \
		/* Disable capture mode */\
		TBCCTL6 = 0; \
		TACCTL2 = 0; \
} while (0)

/** @} */

/* -------------------------------- SFD ----------------------------- */

/**
 * \defgroup chaos_sfd Management of SFD interrupts
 * @{
 */

/**
 * \brief Capture instants of SFD events on timer B1
 * \param edge Edge used for capture.
 *
 */
#define SFD_CAP_INIT(edge) do {\
	P4SEL |= BV(SFD);\
	TBCCTL1 = edge | CAP | SCS;\
} while (0)

/**
 * \brief Enable generation of interrupts due to SFD events
 */
#define ENABLE_SFD_INT()		do { TBCCTL1 |= CCIE; } while (0)

/**
 * \brief Disable generation of interrupts due to SFD events
 */
#define DISABLE_SFD_INT()		do { TBCCTL1 &= ~CCIE; } while (0)

/**
 * \brief Clear interrupt flag due to SFD events
 */
#define CLEAR_SFD_INT()			do { TBCCTL1 &= ~CCIFG; } while (0)

/**
 * \brief Check if generation of interrupts due to SFD events is enabled
 */
#define IS_ENABLED_SFD_INT()    !!(TBCCTL1 & CCIE)

/** @} */

#define SET_PIN(a,b)          do { P##a##OUT |=  BV(b); } while (0)
#define UNSET_PIN(a,b)        do { P##a##OUT &= ~BV(b); } while (0)
#define TOGGLE_PIN(a,b)       do { P##a##OUT ^=  BV(b); } while (0)
#define INIT_PIN_IN(a,b)      do { P##a##SEL &= ~BV(b); P##a##DIR &= ~BV(b); } while (0)
#define INIT_PIN_OUT(a,b)     do { P##a##SEL &= ~BV(b); P##a##DIR |=  BV(b); } while (0)
#define PIN_IS_SET(a,b)       (    P##a##IN  &   BV(b))

// UserINT (P2.7)
#define SET_PIN_USERINT      SET_PIN(2,7)
#define UNSET_PIN_USERINT    UNSET_PIN(2,7)
#define TOGGLE_PIN_USERINT   TOGGLE_PIN(2,7)
#define INIT_PIN_USERINT_IN  INIT_PIN_IN(2,7)
#define INIT_PIN_USERINT_OUT INIT_PIN_OUT(2,7)
#define PIN_USERINT_IS_SET   PIN_IS_SET(2,7)

// GIO2 (P2.3)
#define SET_PIN_GIO2         SET_PIN(2,3)
#define UNSET_PIN_GIO2       UNSET_PIN(2,3)
#define TOGGLE_PIN_GIO2      TOGGLE_PIN(2,3)
#define INIT_PIN_GIO2_IN     INIT_PIN_IN(2,3)
#define INIT_PIN_GIO2_OUT    INIT_PIN_OUT(2,3)
#define PIN_GIO2_IS_SET      PIN_IS_SET(2,3)

// ADC0 (P6.0)
#define SET_PIN_ADC0         SET_PIN(6,0)
#define UNSET_PIN_ADC0       UNSET_PIN(6,0)
#define TOGGLE_PIN_ADC0      TOGGLE_PIN(6,0)
#define INIT_PIN_ADC0_IN     INIT_PIN_IN(6,0)
#define INIT_PIN_ADC0_OUT    INIT_PIN_OUT(6,0)
#define PIN_ADC0_IS_SET      PIN_IS_SET(6,0)

// ADC1 (P6.1)
#define SET_PIN_ADC1         SET_PIN(6,1)
#define UNSET_PIN_ADC1       UNSET_PIN(6,1)
#define TOGGLE_PIN_ADC1      TOGGLE_PIN(6,1)
#define INIT_PIN_ADC1_IN     INIT_PIN_IN(6,1)
#define INIT_PIN_ADC1_OUT    INIT_PIN_OUT(6,1)
#define PIN_ADC1_IS_SET      PIN_IS_SET(6,1)

// ADC2 (P6.2) -> LED3
#define SET_PIN_ADC2         SET_PIN(6,2)
#define UNSET_PIN_ADC2       UNSET_PIN(6,2)
#define TOGGLE_PIN_ADC2      TOGGLE_PIN(6,2)
#define INIT_PIN_ADC2_IN     INIT_PIN_IN(6,2)
#define INIT_PIN_ADC2_OUT    INIT_PIN_OUT(6,2)
#define PIN_ADC2_IS_SET      PIN_IS_SET(6,2)

// ADC6 (P6.6) -> LED2
#define SET_PIN_ADC6         SET_PIN(6,6)
#define UNSET_PIN_ADC6       UNSET_PIN(6,6)
#define TOGGLE_PIN_ADC6      TOGGLE_PIN(6,6)
#define INIT_PIN_ADC6_IN     INIT_PIN_IN(6,6)
#define INIT_PIN_ADC6_OUT    INIT_PIN_OUT(6,6)
#define PIN_ADC6_IS_SET      PIN_IS_SET(6,6)

// ADC7 (P6.7) -> LED1
#define SET_PIN_ADC7         SET_PIN(6,7)
#define UNSET_PIN_ADC7       UNSET_PIN(6,7)
#define TOGGLE_PIN_ADC7      TOGGLE_PIN(6,7)
#define INIT_PIN_ADC7_IN     INIT_PIN_IN(6,7)
#define INIT_PIN_ADC7_OUT    INIT_PIN_OUT(6,7)
#define PIN_ADC7_IS_SET      PIN_IS_SET(6,7)

#endif /* CHAOS_H_ */

/** @} */
