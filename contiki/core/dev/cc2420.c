/*
 * Copyright (c) 2007, Swedish Institute of Computer Science
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
 * @(#)$Id: cc2420.c,v 1.51 2010/04/08 18:23:24 adamdunkels Exp $
 */
/*
 * This code is almost device independent and should be easy to port.
 */

#include <stdio.h>
#include <string.h>

#include "contiki.h"

#include "dev/leds.h"
#include "dev/spi.h"
#include "dev/cc2420.h"
#include "dev/cc2420_const.h"
#include "chaos.h"

/*---------------------------------------------------------------------------*/
#define AUTOACK (1 << 4)
#define ADR_DECODE (1 << 11)
#define RXFIFO_PROTECTION (1 << 9)
#define CORR_THR(n) (((n) & 0x1f) << 6)
#define FIFOP_THR(n) ((n) & 0x7f)
#define RXBPF_LOCUR (1 << 13);
/*---------------------------------------------------------------------------*/
static inline uint8_t radio_status(void) {
	uint8_t status;
	FASTSPI_UPD_STATUS(status);
	return status;
}
static inline void radio_flush_rx(void) {
	uint8_t dummy;
	FASTSPI_READ_FIFO_BYTE(dummy);
	FASTSPI_STROBE(CC2420_SFLUSHRX);
	FASTSPI_STROBE(CC2420_SFLUSHRX);
}
/*---------------------------------------------------------------------------*/
int
cc2420_init(void)
{
  uint16_t reg;
  {
    int s = splhigh();
    spi_init();

    /* all input by default, set these as output */
    P4DIR |= BV(CSN) | BV(VREG_EN) | BV(RESET_N);

    SPI_DISABLE();                /* Unselect radio. */
    DISABLE_FIFOP_INT();
    FIFOP_INT_INIT();
    splx(s);
  }

  /* Turn on voltage regulator and reset. */
  SET_VREG_ACTIVE();
  SET_RESET_ACTIVE();
  clock_delay(127);
  SET_RESET_INACTIVE();

  /* Turn on the crystal oscillator. */
  FASTSPI_STROBE(CC2420_SXOSCON);

  /* Turn off automatic packet acknowledgment and address decoding. */
  FASTSPI_GETREG(CC2420_MDMCTRL0, reg);
  reg &= ~(AUTOACK | ADR_DECODE);
  FASTSPI_SETREG(CC2420_MDMCTRL0, reg);

  /* Change default values as recomended in the data sheet, */
  /* correlation threshold = 20, RX bandpass filter = 1.3uA. */
  FASTSPI_SETREG(CC2420_MDMCTRL1, CORR_THR(20));
  FASTSPI_GETREG(CC2420_RXCTRL1, reg);
  reg |= RXBPF_LOCUR;
  FASTSPI_SETREG(CC2420_RXCTRL1, reg);

  /* Set the FIFOP threshold to maximum. */
  FASTSPI_SETREG(CC2420_IOCFG0, FIFOP_THR(127));

  /* Turn off "Security enable" (page 32). */
  FASTSPI_GETREG(CC2420_SECCTRL0, reg);
  reg &= ~RXFIFO_PROTECTION;
  FASTSPI_SETREG(CC2420_SECCTRL0, reg);

  radio_flush_rx();

  cc2420_set_tx_power(CC2420_TXPOWER);

  return 1;
}
/*---------------------------------------------------------------------------*/
void
cc2420_set_channel(int c)
{
  uint16_t f;

  /*
   * Subtract the base channel (11), multiply by 5, which is the
   * channel spacing. 357 is 2405-2048 and 0x4000 is LOCK_THR = 1.
   */

  f = 5 * (c - 11) + 357 + 0x4000;
  /*
   * Writing RAM requires crystal oscillator to be stable.
   */
  while(!(radio_status() & (BV(CC2420_XOSC16M_STABLE))));

  /* Wait for any transmission to end. */
  while(radio_status() & BV(CC2420_TX_ACTIVE));

  FASTSPI_SETREG(CC2420_FSCTRL, f);
}

void cc2420_set_tx_power(int power) {
	uint16_t reg;
	if( power > CC2420_TXPOWER_MAX ) power = CC2420_TXPOWER_MAX;
	if( power < CC2420_TXPOWER_MIN ) power = CC2420_TXPOWER_MIN;
	FASTSPI_GETREG(CC2420_TXCTRL, reg);
	reg = (reg & 0xffe0) | (power & 0x1f);
	FASTSPI_SETREG(CC2420_TXCTRL, reg);
}
