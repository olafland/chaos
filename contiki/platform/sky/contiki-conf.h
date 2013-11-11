/* -*- C -*- */
/* @(#)$Id: contiki-conf.h,v 1.76 2010/03/19 13:27:46 adamdunkels Exp $ */

#ifndef CONTIKI_CONF_H
#define CONTIKI_CONF_H

#include "testbed.h"
// set COOJA to 1 for simulating Chaos in Cooja
#ifndef COOJA
#define COOJA 1
//#define COOJA 1
#endif

#ifndef TINYOS_SERIAL_FRAMES
#define TINYOS_SERIAL_FRAMES 0
#endif /* TINYOS_SERIAL_FRAMES */

#ifndef RF_CHANNEL
#define RF_CHANNEL              26
#endif /* RF_CHANNEL */

#define ENERGEST_CONF_ON 1

#define HAVE_STDINT_H
#define MSP430_MEMCPY_WORKAROUND 1
#include "msp430def.h"

#define CCIF
#define CLIF

#define PROCESS_CONF_NUMEVENTS 8
#define PROCESS_CONF_STATS 1

/* CPU target speed in Hz */
#if COOJA
#define F_CPU 3900000uL /*2457600uL*/
#else
#define F_CPU 4194304uL /*2457600uL*/
#endif

/* Our clock resolution, this is the same as Unix HZ. */
#define CLOCK_CONF_SECOND 128UL

#define BAUD2UBR(baud) ((F_CPU/baud))

/*
 * Definitions below are dictated by the hardware and not really
 * changeable!
 */

/* LED ports */
#define LEDS_PxDIR P5DIR
#define LEDS_PxOUT P5OUT
#define LEDS_CONF_RED    0x10
#define LEDS_CONF_GREEN  0x20
#define LEDS_CONF_YELLOW 0x40

typedef unsigned long clock_time_t;

typedef unsigned long off_t;
#define ROM_ERASE_UNIT_SIZE  512
#define XMEM_ERASE_UNIT_SIZE (64*1024L)

/* Use the first 64k of external flash for node configuration */
#define NODE_ID_XMEM_OFFSET     (0 * XMEM_ERASE_UNIT_SIZE)

/*
 * SPI bus configuration for the TMote Sky.
 */

/* SPI input/output registers. */
#define SPI_TXBUF U0TXBUF
#define SPI_RXBUF U0RXBUF

				/* USART0 Tx ready? */
#define	SPI_WAITFOREOTx() while ((U0TCTL & TXEPT) == 0)
				/* USART0 Rx ready? */
#define	SPI_WAITFOREORx() while ((IFG1 & URXIFG0) == 0)
				/* USART0 Tx buffer ready? */
#define SPI_WAITFORTxREADY() while ((IFG1 & UTXIFG0) == 0)

#define SCK            1  /* P3.1 - Output: SPI Serial Clock (SCLK) */
#define MOSI           2  /* P3.2 - Output: SPI Master out - slave in (MOSI) */
#define MISO           3  /* P3.3 - Input:  SPI Master in - slave out (MISO) */

/*
 * SPI bus - M25P80 external flash configuration.
 */

#define FLASH_PWR	3	/* P4.3 Output */
#define FLASH_CS	4	/* P4.4 Output */
#define FLASH_HOLD	7	/* P4.7 Output */

/* Enable/disable flash access to the SPI bus (active low). */

#define SPI_FLASH_ENABLE()  ( P4OUT &= ~BV(FLASH_CS) )
#define SPI_FLASH_DISABLE() ( P4OUT |=  BV(FLASH_CS) )

#define SPI_FLASH_HOLD()		( P4OUT &= ~BV(FLASH_HOLD) )
#define SPI_FLASH_UNHOLD()		( P4OUT |=  BV(FLASH_HOLD) )

/*
 * SPI bus - CC2420 pin configuration.
 */

#define FIFO_P         0  /* P1.0 - Input: FIFOP from CC2420 */
#define FIFO           3  /* P1.3 - Input: FIFO from CC2420 */
#define CCA            4  /* P1.4 - Input: CCA from CC2420 */

#define SFD            1  /* P4.1 - Input:  SFD from CC2420 */
#define CSN            2  /* P4.2 - Output: SPI Chip Select (CS_N) */
#define VREG_EN        5  /* P4.5 - Output: VREG_EN to CC2420 */
#define RESET_N        6  /* P4.6 - Output: RESET_N to CC2420 */

/* Pin status. */

#define FIFO_IS_1       (!!(P1IN & BV(FIFO)))
#define CCA_IS_1        (!!(P1IN & BV(CCA) ))
#define RESET_IS_1      (!!(P4IN & BV(RESET_N)))
#define VREG_IS_1       (!!(P4IN & BV(VREG_EN)))
#define FIFOP_IS_1      (!!(P1IN & BV(FIFO_P)))
#define SFD_IS_1        (!!(P4IN & BV(SFD)))

/* The CC2420 reset pin. */
#define SET_RESET_INACTIVE()    ( P4OUT |=  BV(RESET_N) )
#define SET_RESET_ACTIVE()      ( P4OUT &= ~BV(RESET_N) )

/* CC2420 voltage regulator enable pin. */
#define SET_VREG_ACTIVE()       ( P4OUT |=  BV(VREG_EN) )
#define SET_VREG_INACTIVE()     ( P4OUT &= ~BV(VREG_EN) )

/* CC2420 rising edge trigger for external interrupt 0 (FIFOP). */
#define FIFOP_INT_INIT() do {\
  P1IES &= ~BV(FIFO_P);\
  CLEAR_FIFOP_INT();\
} while (0)

/* FIFOP on external interrupt 0. */
#define ENABLE_FIFOP_INT()          do { P1IE |= BV(FIFO_P); } while (0)
#define DISABLE_FIFOP_INT()         do { P1IE &= ~BV(FIFO_P); } while (0)
#define CLEAR_FIFOP_INT()           do { P1IFG &= ~BV(FIFO_P); } while (0)

/* Enables/disables CC2420 access to the SPI bus (not the bus).
 *
 * These guys should really be renamed but are compatible with the
 * original Chipcon naming.
 *
 * SPI_CC2420_ENABLE/SPI_CC2420_DISABLE???
 * CC2420_ENABLE_SPI/CC2420_DISABLE_SPI???
 */

#define SPI_ENABLE()    ( P4OUT &= ~BV(CSN) ) /* ENABLE CSn (active low) */
#define SPI_DISABLE()   ( P4OUT |=  BV(CSN) ) /* DISABLE CSn (active low) */
#define SPI_IS_ENABLED()   ( (P4OUT & BV(CSN)) != BV(CSN) )

#ifdef PROJECT_CONF_H
#include PROJECT_CONF_H
#endif /* PROJECT_CONF_H */



#endif /* CONTIKI_CONF_H */
