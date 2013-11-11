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
 *         Chaos core, source file.
 * \author
 *         Olaf Landsiedel <olafl@chalmers.se>
 * \author
 *         Federico Ferrari <ferrari@tik.ee.ethz.ch>
 */

#include "chaos.h"
#include "chaos-test.h"

/**
 * \brief a bunch of define for gcc 4.6
 */
#define CM_POS              CM_1
#define CM_NEG              CM_2
#define CM_BOTH             CM_3

static uint8_t initiator, /*sync,*/ rx_cnt, tx_cnt, tx_max;
static uint8_t *data, *packet;
//static uint8_t data_len, packet_len;
static uint8_t bytes_read, tx_relay_cnt_last;
static volatile uint8_t state;
static rtimer_clock_t t_rx_start, t_rx_stop, t_tx_start, t_tx_stop;
static rtimer_clock_t t_rx_timeout;
static rtimer_clock_t T_irq;
static unsigned short ie1, ie2, p1ie, p2ie, tbiv;
static uint8_t tx;
static uint8_t chaos_complete;
static uint8_t tx_cnt_complete;
static uint8_t estimate_length;
static rtimer_clock_t t_timeout_start, t_timeout_stop, now, tbccr1;
static uint32_t T_timeout_h;
static uint16_t n_timeout_wait;
static uint8_t n_slots_timeout, relay_cnt_timeout;

static rtimer_clock_t T_slot_h = 0, T_rx_h, T_w_rt_h, T_tx_h, T_w_tr_h, t_ref_l, T_offset_h, t_first_rx_l;
#if CHAOS_SYNC_WINDOW
static unsigned long T_slot_h_sum;
static uint8_t win_cnt;
#endif /* CHAOS_SYNC_WINDOW */
static uint8_t relay_cnt, t_ref_l_updated;

#ifdef LOG_TIRQ
#define CHAOS_TIRQ_LOG_SIZE 70
static uint8_t tirq_log_cnt;
static rtimer_clock_t tirq_log[CHAOS_TIRQ_LOG_SIZE];
#endif //LOG_TIRQ

#ifdef LOG_FLAGS
#define CHAOS_FLAGS_LOG_SIZE 70
static uint16_t flags_tx_cnt;
static uint8_t flags_tx[CHAOS_FLAGS_LOG_SIZE*MERGE_LEN];
static uint8_t relay_counts_tx[CHAOS_FLAGS_LOG_SIZE];
static uint16_t flags_rx_cnt;
static uint8_t flags_rx[CHAOS_FLAGS_LOG_SIZE*MERGE_LEN];
static uint8_t relay_counts_rx[CHAOS_FLAGS_LOG_SIZE];
#ifdef LOG_ALL_FLAGS
static uint8_t current_flags_rx[MERGE_LEN];
#endif /* LOG_ALL_FLAGS */
#endif /* LOG_FLAGS */

static inline void chaos_schedule_timeout(void) {
	if (T_slot_h && TIMEOUT) {
		// random number between MIN_SLOTS_TIMEOUT and MAX_SLOTS_TIMEOUT
		n_slots_timeout = MIN_SLOTS_TIMEOUT + ((RTIMER_NOW() + RTIMER_NOW_DCO()) % (MAX_SLOTS_TIMEOUT - MIN_SLOTS_TIMEOUT + 1));
		T_timeout_h = n_slots_timeout * (uint32_t)T_slot_h;
		t_timeout_stop = t_timeout_start + T_timeout_h;
		if (T_timeout_h >> 16) {
			now = RTIMER_NOW_DCO();
			n_timeout_wait = (T_timeout_h - (now - t_timeout_start)) >> 16;
	        // it should never happen, but to be sure...
			if (n_timeout_wait == 0xffff) {
				n_timeout_wait = 0;
			}
		} else {
			n_timeout_wait = 0;
		}
		TBCCR4 = t_timeout_stop;
		TBCCTL4 = CCIE;
		SET_PIN_ADC6;
	}
}

static inline void chaos_stop_timeout(void) {
	TBCCTL4 = 0;
	n_timeout_wait = 0;
	UNSET_PIN_ADC6;
}

/* --------------------------- Radio functions ---------------------- */
static inline void radio_flush_tx(void) {
	FASTSPI_STROBE(CC2420_SFLUSHTX);
}

static inline uint8_t radio_status(void) {
	uint8_t status;
	FASTSPI_UPD_STATUS(status);
	return status;
}

static inline void radio_on(void) {
	FASTSPI_STROBE(CC2420_SRXON);
	while(!(radio_status() & (BV(CC2420_XOSC16M_STABLE))));
	SET_PIN_ADC0;
	ENERGEST_ON(ENERGEST_TYPE_LISTEN);
}

static inline void radio_off(void) {
#if ENERGEST_CONF_ON
	if (energest_current_mode[ENERGEST_TYPE_TRANSMIT]) {
		ENERGEST_OFF(ENERGEST_TYPE_TRANSMIT);
	}
	if (energest_current_mode[ENERGEST_TYPE_LISTEN]) {
		ENERGEST_OFF(ENERGEST_TYPE_LISTEN);
	}
#endif /* ENERGEST_CONF_ON */
	UNSET_PIN_ADC0;
	UNSET_PIN_ADC1;
	UNSET_PIN_ADC2;
	FASTSPI_STROBE(CC2420_SRFOFF);
	chaos_stop_timeout();
}

static inline void radio_flush_rx(void) {
	uint8_t dummy;
	FASTSPI_READ_FIFO_BYTE(dummy);
	FASTSPI_STROBE(CC2420_SFLUSHRX);
	FASTSPI_STROBE(CC2420_SFLUSHRX);
}

static inline void radio_abort_rx(void) {
	state = CHAOS_STATE_ABORTED;
	UNSET_PIN_ADC1;
	radio_flush_rx();
}

static inline void radio_abort_tx(void) {
	UNSET_PIN_ADC2;
	FASTSPI_STROBE(CC2420_SRXON);
#if ENERGEST_CONF_ON
	if (energest_current_mode[ENERGEST_TYPE_TRANSMIT]) {
		ENERGEST_OFF(ENERGEST_TYPE_TRANSMIT);
		ENERGEST_ON(ENERGEST_TYPE_LISTEN);
	}
#endif /* ENERGEST_CONF_ON */
	radio_flush_rx();
}

static inline void radio_start_tx(void) {
	t_timeout_start = RTIMER_NOW_DCO();
	FASTSPI_STROBE(CC2420_STXON);
	SET_PIN_ADC0;
	UNSET_PIN_ADC1;
#if ENERGEST_CONF_ON
	ENERGEST_OFF(ENERGEST_TYPE_LISTEN);
	ENERGEST_ON(ENERGEST_TYPE_TRANSMIT);
#endif /* ENERGEST_CONF_ON */
	chaos_schedule_timeout();
}

static inline void radio_write_tx(void) {
	FASTSPI_WRITE_FIFO(packet, PACKET_LEN - 1);
}

void chaos_data_processing(void){
	chaos_data_struct* local = (chaos_data_struct*)data;
	chaos_data_struct* received = (chaos_data_struct*)(&CHAOS_DATA_FIELD);

	uint8_t complete_temp = 0xFF;
	uint16_t i;
	for( i = 0; i < MERGE_LEN-1; i++){
		uint8_t local_flag = local->flags[i];
#if defined LOG_FLAGS && defined LOG_ALL_FLAGS
		current_flags_rx[i] = received->flags[i];
#endif /* LOG_FLAGS */
		tx |= (received->flags[i] != local_flag);
		received->flags[i] |= local_flag;
		complete_temp &= received->flags[i];
	}
	uint8_t local_flag = local->flags[MERGE_LEN-1];
#if defined LOG_FLAGS && defined LOG_ALL_FLAGS
	current_flags_rx[MERGE_LEN-1] = received->flags[MERGE_LEN-1];
#endif /* LOG_FLAGS */
	tx |= (received->flags[MERGE_LEN-1] != local_flag);
	received->flags[MERGE_LEN-1] |= local_flag;
	chaos_complete = (complete_temp == 0xFF) && (received->flags[MERGE_LEN-1] == CHAOS_COMPLETE_FLAG);

//	for( i=0; i < PAYLOAD_LEN; i++ ){
//		received->payload[i] = (received->payload[i] > local->payload[i]) ? received->payload[i] : local->payload[i];
//	}

//	random processing
//	uint16_t tmp = 0;
//	if (RTIMER_NOW_DCO() % 14) {
//		tmp = 12;
//		if (RTIMER_NOW() % 15) {
//			tmp++;
//		}
//	} else {
//		tmp = 100;
//		asm volatile("nop");
//	}
//	tmp++;

}


/* --------------------------- SFD interrupt ------------------------ */
interrupt(TIMERB1_VECTOR) __attribute__ ((section(".chaos")))
timerb1_interrupt(void)
{

	if (state == CHAOS_STATE_RECEIVING && !SFD_IS_1) {
		// packet reception has finished

		// store the time at which the SFD was captured into an ad hoc variable
		// (the value of TBCCR1 might change in case other SFD interrupts occur)
		tbccr1 = TBCCR1;

		UNSET_PIN_ADC1;

		// read the remaining bytes from the RXFIFO
		FASTSPI_READ_FIFO_NO_WAIT(&packet[bytes_read], PACKET_LEN - bytes_read + 1);
		bytes_read = PACKET_LEN + 1;

		if (CHAOS_CRC_FIELD & FOOTER1_CRC_OK) {
			// CRC ok: packet successfully received
			SET_PIN_ADC7;
			// stop the timeout
			chaos_stop_timeout();
			// data processing
			chaos_data_processing();

			//ok, data processing etc is done and we are ready to transmit a packet
			//now the black magic part starts:
			//we ensure synchronous transmissions on all transmitting Chaos nodes
			//by making them transmit the data packet at exactly the same number of
			//processor cycles after the receive
			//this consists of two steps:
			//1. we wait until a defined time is reached (wait loop)
			//2. we execute a NOP loop to ensure precise synchronization at CPU instruction cycles.
			//(step two is the same as in Glossy)

			// wait loop
			// wait until PROCESSING_CYCLES cycles occurred since the last SFD event
			TBCCR4 = tbccr1 + PROCESSING_CYCLES;
			do {
				TBCCTL4 |= CCIE;
			} while (!(TBCCTL4 & CCIFG));
			TBCCTL4 = 0;

			UNSET_PIN_ADC7;

			// the next instruction is executed at least PROCESSING_CYCLES+12 cycles since the last SFD event
			// ->achieve basic clock synchronization for synchronous TX

			//prepare for NOP loop
			//compute interrupt etc. delay to do get instruction level synchronization for TX
			T_irq = ((RTIMER_NOW_DCO() - tbccr1) - (PROCESSING_CYCLES+15)) << 1;

			// NOP loop: slip stream!!
			// if delay is within reasonable range: execute NOP loop do ensure synchronous TX
			// T_irq in [0,...,34]
			if (T_irq <= 34) {
				if (tx) {
					// NOPs (variable number) to compensate for the interrupt service and the busy waiting delay
					asm volatile("add %[d], r0" : : [d] "m" (T_irq));
					asm volatile("nop");						// irq_delay = 0
					asm volatile("nop");						// irq_delay = 2
					asm volatile("nop");						// irq_delay = 4
					asm volatile("nop");						// irq_delay = 6
					asm volatile("nop");						// irq_delay = 8
					asm volatile("nop");						// irq_delay = 10
					asm volatile("nop");						// irq_delay = 12
					asm volatile("nop");						// irq_delay = 14
					asm volatile("nop");						// irq_delay = 16
					asm volatile("nop");						// irq_delay = 18
					asm volatile("nop");						// irq_delay = 20
					asm volatile("nop");						// irq_delay = 22
					asm volatile("nop");						// irq_delay = 24
					asm volatile("nop");						// irq_delay = 26
					asm volatile("nop");						// irq_delay = 28
					asm volatile("nop");						// irq_delay = 30
					asm volatile("nop");						// irq_delay = 32
					asm volatile("nop");						// irq_delay = 34
					// relay the packet
					//
					// -> all transmitting nodes have instruction level synchronization
					// with
					radio_start_tx();
				}
				// read TBIV to clear IFG
				tbiv = TBIV;
				chaos_end_rx();
			} else {
				// interrupt service delay is too high: do not relay the packet
				// FF: this should never happen!
				leds_toggle(LEDS_RED);
#if CHAOS_DEBUG
				if (tx) {
					high_T_irq++;
				}
#endif
				tx = 0;
				// read TBIV to clear IFG
				tbiv = TBIV;
				chaos_end_rx();
			}
		} else {
			// CRC not ok
#if CHAOS_DEBUG
			bad_crc++;
#endif /* CHAOS_DEBUG */
			tx = 0;
			// read TBIV to clear IFG
			tbiv = TBIV;
			chaos_end_rx();
		}
	} else {
		// read TBIV to clear IFG
		tbiv = TBIV;
		if (state == CHAOS_STATE_WAITING && SFD_IS_1) {
			// packet reception has started
			chaos_begin_rx();
		} else {
			if (state == CHAOS_STATE_RECEIVED && SFD_IS_1) {
				// packet transmission has started
				chaos_begin_tx();
			} else {
				if (state == CHAOS_STATE_TRANSMITTING && !SFD_IS_1) {
					// packet transmission has finished
					chaos_end_tx();
				} else {
					if (state == CHAOS_STATE_ABORTED) {
						// packet reception has been aborted
						state = CHAOS_STATE_WAITING;
					} else {
						if (tbiv == TBIV_TBCCR4) {
							// timeout
							if (n_timeout_wait > 0) {
								n_timeout_wait--;
							} else {
								if (state == CHAOS_STATE_WAITING) {
									// start another transmission
									radio_start_tx();
									UNSET_PIN_ADC6;
									if (initiator && rx_cnt == 0) {
										CHAOS_LEN_FIELD = PACKET_LEN;
										CHAOS_HEADER_FIELD = CHAOS_HEADER;
									} else {
										// stop estimating the slot length during this round (to keep maximum precision)
										estimate_length = 0;
										CHAOS_LEN_FIELD = PACKET_LEN;
										CHAOS_HEADER_FIELD = CHAOS_HEADER+1;
									}
									CHAOS_RELAY_CNT_FIELD = relay_cnt_timeout;
									if (DATA_LEN > BYTES_TIMEOUT) {
										// first BYTES_TIMEOUT bytes
										memcpy(&CHAOS_DATA_FIELD, data, BYTES_TIMEOUT);
										radio_flush_rx();
										FASTSPI_WRITE_FIFO(packet, BYTES_TIMEOUT + 1 + CHAOS_HEADER_LEN);
										// remaining bytes
										memcpy(&CHAOS_BYTES_TIMEOUT_FIELD, &data[BYTES_TIMEOUT], DATA_LEN - BYTES_TIMEOUT);
										FASTSPI_WRITE_FIFO(&packet[BYTES_TIMEOUT + 1 + CHAOS_HEADER_LEN], PACKET_LEN - BYTES_TIMEOUT - 1 - CHAOS_HEADER_LEN - 1);
									} else {
										memcpy(&CHAOS_DATA_FIELD, data, DATA_LEN);
										// write the packet to the TXFIFO
										radio_flush_rx();
										radio_write_tx();
									}
									state = CHAOS_STATE_RECEIVED;
								} else {
									// stop the timeout
									chaos_stop_timeout();
								}
							}
						} else {
							if (state != CHAOS_STATE_OFF) {
								// something strange is going on: go back to the waiting state
								radio_flush_rx();
								// stop the timeout
								chaos_stop_timeout();
								state = CHAOS_STATE_WAITING;
							}
						}
					}
				}
			}
		}
	}
}

/* --------------------------- Chaos process ----------------------- */
PROCESS(chaos_process, "Chaos busy-waiting process");
PROCESS_THREAD(chaos_process, ev, data) {
	PROCESS_BEGIN();

	// initialize output GPIO pins:
	// radio on
	INIT_PIN_ADC0_OUT;
	// packet Rx
	INIT_PIN_ADC1_OUT;
	// packet Tx
	INIT_PIN_ADC2_OUT;
	// Rx or Tx failure
	INIT_PIN_ADC6_OUT;
	// successful packet Rx
	INIT_PIN_ADC7_OUT;

	while (1) {
		PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
		// prevent the Contiki main cycle to enter the LPM mode or
		// any other process to run while Chaos is running
		while (CHAOS_IS_ON());
	}

	PROCESS_END();
}

static inline void chaos_disable_other_interrupts(void) {
    int s = splhigh();
	ie1 = IE1;
	ie2 = IE2;
	p1ie = P1IE;
	p2ie = P2IE;
	IE1 = 0;
	IE2 = 0;
	P1IE = 0;
	P2IE = 0;
	CACTL1 &= ~CAIE;
	DMA0CTL &= ~DMAIE;
	DMA1CTL &= ~DMAIE;
	DMA2CTL &= ~DMAIE;
	// disable etimer interrupts
	TACCTL1 &= ~CCIE;
	TBCCTL0 = 0;
	DISABLE_FIFOP_INT();
	CLEAR_FIFOP_INT();
	SFD_CAP_INIT(CM_BOTH);
	ENABLE_SFD_INT();
	// stop Timer B
	TBCTL = 0;
	// Timer B sourced by the DCO
	TBCTL = TBSSEL1;
	// start Timer B
	TBCTL |= MC1;
    splx(s);
    watchdog_stop();
}

static inline void chaos_enable_other_interrupts(void) {
	int s = splhigh();
	IE1 = ie1;
	IE2 = ie2;
	P1IE = p1ie;
	P2IE = p2ie;
	// enable etimer interrupts
	TACCTL1 |= CCIE;
#if COOJA
	if (TACCTL1 & CCIFG) {
		etimer_interrupt();
	}
#endif
	DISABLE_SFD_INT();
	CLEAR_SFD_INT();
	FIFOP_INT_INIT();
	ENABLE_FIFOP_INT();
	// stop Timer B
	TBCTL = 0;
	// Timer B sourced by the 32 kHz
	TBCTL = TBSSEL0;
	// start Timer B
	TBCTL |= MC1;
    splx(s);
    watchdog_start();
}

/* --------------------------- Main interface ----------------------- */
void chaos_start(uint8_t *data_, /*uint8_t data_len_,*/ uint8_t initiator_,
		/*uint8_t sync_,*/ uint8_t tx_max_) {
	// copy function arguments to the respective Chaos variables
	data = data_;
//	data_len = data_len_;
	initiator = initiator_;
//	sync = sync_;
	tx_max = tx_max_;
	// disable all interrupts that may interfere with Chaos
	chaos_disable_other_interrupts();
	// initialize Chaos variables
	tx_cnt = 0;
	rx_cnt = 0;

	chaos_complete = CHAOS_INCOMPLETE;
	tx_cnt_complete = 0;
	estimate_length = 1;

#if CHAOS_DEBUG
	rc_update = 0;
#endif /* CHAOS_DEBUG */
	// set Chaos packet length, with or without relay counter depending on the sync flag value
//	packet_len = (CHAOS_SYNC_MODE) ?
//			DATA_LEN + FOOTER_LEN + CHAOS_RELAY_CNT_LEN + CHAOS_HEADER_LEN :
//			DATA_LEN + FOOTER_LEN + CHAOS_HEADER_LEN;
	// allocate memory for the temporary buffer
	packet = (uint8_t *) malloc(PACKET_LEN + 1);
	// set the packet length field to the appropriate value
	CHAOS_LEN_FIELD = PACKET_LEN;
	// set the header field
	CHAOS_HEADER_FIELD = CHAOS_HEADER;
	if (initiator) {
		// initiator: copy the application data to the data field
		//OL: from local data to packet that will be tx
		memcpy(&CHAOS_DATA_FIELD, data, DATA_LEN);
		// set Chaos state
		state = CHAOS_STATE_RECEIVED;
	} else {
		// receiver: set Chaos state
		state = CHAOS_STATE_WAITING;
	}
	if (CHAOS_SYNC_MODE) {
		// set the relay_cnt field to 0
		CHAOS_RELAY_CNT_FIELD = 0;
		// the reference time has not been updated yet
		t_ref_l_updated = 0;
	}

#if !COOJA
	// resynchronize the DCO
	msp430_sync_dco();
#endif /* COOJA */

	// flush radio buffers
	radio_flush_rx();
	radio_flush_tx();
	if (initiator) {
		// write the packet to the TXFIFO
		radio_write_tx();
		// start the first transmission
		radio_start_tx();
	} else {
		// turn on the radio
		radio_on();
	}
	// activate the Chaos busy waiting process
	process_poll(&chaos_process);
}

uint8_t chaos_stop(void) {
	// turn off the radio
	radio_off();

	// flush radio buffers
	radio_flush_rx();
	radio_flush_tx();

	state = CHAOS_STATE_OFF;
	// re-enable non Chaos-related interrupts
	chaos_enable_other_interrupts();
	// deallocate memory for the temporary buffer
	free(packet);
	// return the number of times the packet has been received
	return rx_cnt;
}

uint8_t get_rx_cnt(void) {
	return rx_cnt;
}

uint8_t get_relay_cnt(void) {
	return relay_cnt;
}

rtimer_clock_t get_T_slot_h(void) {
	return T_slot_h;
}

uint8_t is_t_ref_l_updated(void) {
	return t_ref_l_updated;
}

rtimer_clock_t get_t_first_rx_l(void) {
	return t_first_rx_l;
}

rtimer_clock_t get_t_ref_l(void) {
	return t_ref_l;
}

void set_t_ref_l(rtimer_clock_t t) {
	t_ref_l = t;
}

void set_t_ref_l_updated(uint8_t updated) {
	t_ref_l_updated = updated;
}

uint8_t get_state(void) {
	return state;
}

static inline void estimate_slot_length(rtimer_clock_t t_rx_stop_tmp) {
	// estimate slot length if rx_cnt > 1
	// and we have received a packet immediately after our last transmission
	if ((rx_cnt > 1) && (CHAOS_RELAY_CNT_FIELD == (tx_relay_cnt_last + 2))) {
		T_w_rt_h = t_tx_start - t_rx_stop;
		T_tx_h = t_tx_stop - t_tx_start;
		T_w_tr_h = t_rx_start - t_tx_stop;
		T_rx_h = t_rx_stop_tmp - t_rx_start;
		uint32_t T_slot_h_tmp = ((uint32_t)T_tx_h + (uint32_t)T_w_tr_h + (uint32_t)T_rx_h + (uint32_t)T_w_rt_h) / 2;
#if CHAOS_SYNC_WINDOW
		T_slot_h_sum += T_slot_h_tmp;
		if ((++win_cnt) == CHAOS_SYNC_WINDOW) {
			// update the slot length estimation
			T_slot_h = T_slot_h_sum / CHAOS_SYNC_WINDOW;
			// halve the counters
			T_slot_h_sum /= 2;
			win_cnt /= 2;
		} else {
			if (win_cnt == 1) {
				// at the beginning, use the first estimation of the slot length
				T_slot_h = T_slot_h_tmp;
			}
		}
		estimate_length = 0;
#if CHAOS_DEBUG
		rc_update = CHAOS_RELAY_CNT_FIELD;
#endif /* CHAOS_DEBUG */
#else
		T_slot_h = T_slot_h_tmp;
#endif /* CHAOS_SYNC_WINDOW */
	}
}

static inline void compute_sync_reference_time(void) {
#if COOJA
	rtimer_clock_t t_cap_l = RTIMER_NOW();
	rtimer_clock_t t_cap_h = RTIMER_NOW_DCO();
#else
	// capture the next low-frequency clock tick
	rtimer_clock_t t_cap_h, t_cap_l;
	CAPTURE_NEXT_CLOCK_TICK(t_cap_h, t_cap_l);
#endif /* COOJA */
	rtimer_clock_t T_rx_to_cap_h = t_cap_h - t_rx_start;
	unsigned long T_ref_to_rx_h = (CHAOS_RELAY_CNT_FIELD - 1) * (unsigned long)T_slot_h;
	unsigned long T_ref_to_cap_h = T_ref_to_rx_h + (unsigned long)T_rx_to_cap_h;
	rtimer_clock_t T_ref_to_cap_l = 1 + T_ref_to_cap_h / CLOCK_PHI;
	// high-resolution offset of the reference time
	T_offset_h = (CLOCK_PHI - 1) - (T_ref_to_cap_h % CLOCK_PHI);
	// low-resolution value of the reference time
	t_ref_l = t_cap_l - T_ref_to_cap_l;
	relay_cnt = CHAOS_RELAY_CNT_FIELD - 1;
	// the reference time has been updated
	t_ref_l_updated = 1;
}

/* ----------------------- Interrupt functions ---------------------- */
inline void chaos_begin_rx(void) {
	SET_PIN_ADC1;
	t_rx_start = TBCCR1;
	state = CHAOS_STATE_RECEIVING;
	// Rx timeout: packet duration + 200 us
	// (packet duration: 32 us * packet_length, 1 DCO tick ~ 0.23 us)
	t_rx_timeout = t_rx_start + ((rtimer_clock_t)PACKET_LEN * 35 + 200) * 4;
	tx = 0;

	// wait until the FIFO pin is 1 (i.e., until the first byte is received)
	while (!FIFO_IS_1) {
		if (!RTIMER_CLOCK_LT(RTIMER_NOW_DCO(), t_rx_timeout)) {
			radio_abort_rx();
#if CHAOS_DEBUG
			rx_timeout++;
#endif /* CHAOS_DEBUG */
			return;
		}
	};
#if COOJA
	//OL: do not ask why
	int i;
	for(i = 0; i < 40; i++){
		asm volatile("nop");
	}
#endif /* COOJA */
	// read the first byte (i.e., the len field) from the RXFIFO
	FASTSPI_READ_FIFO_BYTE(CHAOS_LEN_FIELD);
	// keep receiving only if it has the right length
	if (CHAOS_LEN_FIELD != PACKET_LEN) {
		// packet with a wrong length: abort packet reception
		radio_abort_rx();
#if CHAOS_DEBUG
		bad_length++;
#endif /* CHAOS_DEBUG */
		return;
	}
	bytes_read = 1;

#if FINAL_CHAOS_FLOOD
	//Chaos mode on completion
	if( chaos_complete == CHAOS_COMPLETE && tx_cnt_complete < N_TX_COMPLETE){
		tx = 1;
	}
#endif /* FINAL_CHAOS_FLOOD */

	// wait until the FIFO pin is 1 (i.e., until the second byte is received)
	while (!FIFO_IS_1) {
		if (!RTIMER_CLOCK_LT(RTIMER_NOW_DCO(), t_rx_timeout)) {
			radio_abort_rx();
#if CHAOS_DEBUG
			rx_timeout++;
#endif /* CHAOS_DEBUG */
			return;
		}
	};
	// read the second byte (i.e., the header field) from the RXFIFO
	FASTSPI_READ_FIFO_BYTE(CHAOS_HEADER_FIELD);
	// keep receiving only if it has the right header
	if (CHAOS_HEADER_FIELD < CHAOS_HEADER) {
		// packet with a wrong header: abort packet reception
		radio_abort_rx();
#if CHAOS_DEBUG
		bad_header++;
#endif /* CHAOS_DEBUG */
		return;
	}
	bytes_read = 2;
	if (PACKET_LEN > 8) {
		// if packet is longer than 8 bytes, read all bytes but the last 8
		while (bytes_read <= PACKET_LEN - 8) {
			// wait until the FIFO pin is 1 (until one more byte is received)
			while (!FIFO_IS_1) {
				if (!RTIMER_CLOCK_LT(RTIMER_NOW_DCO(), t_rx_timeout)) {
					radio_abort_rx();
#if CHAOS_DEBUG
					rx_timeout++;
#endif /* CHAOS_DEBUG */
					return;
				}
			};
			// read another byte from the RXFIFO
			FASTSPI_READ_FIFO_BYTE(packet[bytes_read]);
			bytes_read++;
		}
	}
}

inline void chaos_end_rx(void) {
	rtimer_clock_t t_rx_stop_tmp = tbccr1;
	if (tx) {
		// packet correctly received and tx required
		if (CHAOS_SYNC_MODE) {
			// increment relay_cnt field
			CHAOS_RELAY_CNT_FIELD++;
		}
		if (tx_cnt == tx_max) {
			// no more Tx to perform: stop Chaos
			radio_off();
			state = CHAOS_STATE_OFF;
		} else {
			// write Chaos packet to the TXFIFO
			if (chaos_complete == CHAOS_COMPLETE) {
				CHAOS_HEADER_FIELD = CHAOS_HEADER+1;
			}
			radio_flush_rx();
			radio_write_tx();
			state = CHAOS_STATE_RECEIVED;
		}
		if (rx_cnt == 0) {
			// first successful reception: store current time
			t_first_rx_l = RTIMER_NOW();
		}

#ifdef LOG_TIRQ
		if (tirq_log_cnt < CHAOS_TIRQ_LOG_SIZE) {
			tirq_log[tirq_log_cnt] = T_irq;
			tirq_log_cnt++;
		}
#endif

		rx_cnt++;
		if (CHAOS_SYNC_MODE && estimate_length && CHAOS_HEADER_FIELD == CHAOS_HEADER) {
			estimate_slot_length(t_rx_stop_tmp);
		}
		t_rx_stop = t_rx_stop_tmp;
		*(chaos_data_struct*)data = *(chaos_data_struct*)(&CHAOS_DATA_FIELD);
#if FINAL_CHAOS_FLOOD
		if( chaos_complete == CHAOS_COMPLETE ){
			tx_cnt_complete++;
		}
#endif /* FINAL_CHAOS_FLOOD */
#ifdef LOG_FLAGS
		uint8_t i;
#ifdef LOG_ALL_FLAGS
		if (flags_rx_cnt < CHAOS_FLAGS_LOG_SIZE*MERGE_LEN - MERGE_LEN) {
			relay_counts_rx[flags_rx_cnt / MERGE_LEN] = CHAOS_RELAY_CNT_FIELD;
			for (i = 0; i < MERGE_LEN; i++) {
				flags_rx[flags_rx_cnt] = current_flags_rx[i];
				flags_rx_cnt++;
			}
		}
#else
		// store only the first complete reception
		if (chaos_complete == CHAOS_COMPLETE && flags_rx_cnt == 0) {
			relay_counts_rx[flags_rx_cnt / MERGE_LEN] = CHAOS_RELAY_CNT_FIELD;
			for (i = 0; i < MERGE_LEN-1; i++) {
				flags_rx[flags_rx_cnt] = 0xff;
				flags_rx_cnt++;
			}
			flags_rx[flags_rx_cnt] = CHAOS_COMPLETE_FLAG;
			flags_rx_cnt++;
		}
#endif /* LOG_ALL_FLAGS */
#endif /* LOG_FLAGS */
	} else {
		radio_flush_rx();
		state = CHAOS_STATE_WAITING;
	}
}

inline void chaos_begin_tx(void) {
	SET_PIN_ADC2;
	t_tx_start = TBCCR1;
	state = CHAOS_STATE_TRANSMITTING;
	tx_relay_cnt_last = CHAOS_RELAY_CNT_FIELD;
	// relay counter to be used in case the timeout expires
	relay_cnt_timeout = CHAOS_RELAY_CNT_FIELD + n_slots_timeout;

	if ((CHAOS_SYNC_MODE) && (T_slot_h) && (!t_ref_l_updated) && (rx_cnt)) {
		// compute the reference time after the first reception (higher accuracy)
		compute_sync_reference_time();
	}
#ifdef LOG_FLAGS
#ifdef LOG_ALL_FLAGS
	uint8_t i;
	if (flags_tx_cnt < CHAOS_FLAGS_LOG_SIZE*MERGE_LEN - MERGE_LEN) {
		relay_counts_tx[flags_tx_cnt / MERGE_LEN] = CHAOS_RELAY_CNT_FIELD;
		for (i = 0; i < MERGE_LEN; i++) {
			flags_tx[flags_tx_cnt] = ((chaos_data_struct *)data)->flags[i];
			flags_tx_cnt++;
		}
	}
#else
	// store only the last transmission
	uint8_t i;
	flags_tx_cnt = 0;
	relay_counts_tx[flags_tx_cnt / MERGE_LEN] = CHAOS_RELAY_CNT_FIELD;
	for (i = 0; i < MERGE_LEN; i++) {
		flags_tx[flags_tx_cnt] = ((chaos_data_struct *)data)->flags[i];
		flags_tx_cnt++;
	}
#endif /* LOG_ALL_FLAGS */
#endif /* LOG_FLAGS */
}

inline void chaos_end_tx(void) {
	UNSET_PIN_ADC2;
	ENERGEST_OFF(ENERGEST_TYPE_TRANSMIT);
	ENERGEST_ON(ENERGEST_TYPE_LISTEN);
	t_tx_stop = TBCCR1;
	// stop Chaos if tx_cnt reached tx_max (and tx_max > 1 at the initiator, if sync is enabled)
	if ((++tx_cnt == tx_max) && ((!CHAOS_SYNC_MODE) || ((tx_max - initiator) > 0))) {
		radio_off();
		state = CHAOS_STATE_OFF;
#if FINAL_CHAOS_FLOOD
	} else if ( chaos_complete == CHAOS_COMPLETE && tx_cnt_complete >= N_TX_COMPLETE ){
		radio_off();
		state = CHAOS_STATE_OFF;
#endif /* FINAL_CHAOS_FLOOD */
	} else {
		state = CHAOS_STATE_WAITING;
	}
	radio_flush_tx();
}

/* ------------------------------ Timeouts -------------------------- */
#ifdef LOG_TIRQ
inline void print_tirq(void){
	printf("win_cnt %u, T_slot_tmp %u\n", win_cnt, (uint16_t)(T_slot_h_sum / win_cnt));
	printf("tirq %02x:", tirq_log_cnt);
	uint8_t i;
	for( i = 0; i < tirq_log_cnt; i++ ){
		printf("%04x,",tirq_log[i] );
	}
	printf("\n");
	tirq_log_cnt = 0;
	memset(&tirq_log, 0, sizeof(tirq_log));
}
#endif

#ifdef LOG_FLAGS
inline void print_flags_tx(void) {
	printf("flags_tx %2u:", flags_tx_cnt / MERGE_LEN);
	uint8_t i;
	int8_t j;
	for (i = 0; i < flags_tx_cnt / MERGE_LEN; i++) {
		printf("0x%02x-0x%02x-0x", i, relay_counts_tx[i]);
		for (j = MERGE_LEN-1; j >= 0; j--) {
			printf("%02x", flags_tx[i*MERGE_LEN + j]);
		}
		printf(",");
	}
	printf("\n");
	flags_tx_cnt = 0;
	memset(&flags_tx, 0, sizeof(flags_tx));
	memset(&relay_counts_tx, 0, sizeof(relay_counts_tx));
}

inline void print_flags_rx(void) {
	printf("flags_rx %2u:", flags_rx_cnt / MERGE_LEN);
	uint8_t i;
	int8_t j;
	for (i = 0; i < flags_rx_cnt / MERGE_LEN; i++) {
		printf("0x%02x-0x%02x-0x", i, relay_counts_rx[i]);
		for (j = MERGE_LEN-1; j >= 0; j--) {
			printf("%02x", flags_rx[i*MERGE_LEN + j]);
		}
		printf(",");
	}
	printf("\n");
	flags_rx_cnt = 0;
	memset(&flags_rx, 0, sizeof(flags_rx));
	memset(&relay_counts_rx, 0, sizeof(relay_counts_rx));
}
#endif /* LOG_FLAGS */
