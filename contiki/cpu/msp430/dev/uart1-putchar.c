/* This code is adapted from Joris Borms and Fredrik Osterlind.
 * Marco Zimmerling, 2013
 */

/*
 * http://www.tinyos.net/tinyos-2.x/doc/html/tep113.html
 * http://mail.millennium.berkeley.edu/pipermail/tinyos-2-commits/2006-June/003534.html

 SF [ { Pr Seq Disp ( Dest Src len Grp Type | Payload ) } CRC ] EF

 SF      1   0x7E    Start Frame Byte
 Pr      1   0x45    Protocol Byte (SERIAL_PROTO_PACKET_NOACK)
 Seq     0           (not used) Sequence number byte, not used due to SERIAL_PROTO_PACKET_NOACK
 Disp    1   0x00    Packet format dispatch byte (TOS_SERIAL_ACTIVE_MESSAGE_ID)
 Dest    2   0xFFFF  (not used)
 Src     2   0x0000  (not used)
 len     1   N       Payload length
 Grp     1   0x00    Group
 Type    1           Message ID
 Payload N           The actual serial message
 CRC     2           Checksum of {Pr -> end of payload}
 EF      1   0x7E    End Frame Byte
 */

#include "dev/uart1.h"

#if TINYOS_SERIAL_FRAMES

#ifndef UART1_PUTCHAR_CONF_BUF_SIZE
#define UART1_PUTCHAR_BUF_SIZE 100
#else /* UART1_PUTCHAR_CONF_BUF_SIZE */
#define UART1_PUTCHAR_BUF_SIZE UART1_PUTCHAR_CONF_BUF_SIZE
#endif /*n UART1_PUTCHAR_CONF_BUF_SIZE */

#define SYNCH_BYTE 	0x7e
#define ESCAPE_BYTE	0x7d
#define PROT_NOACK	0x45
#define MSG_ID			0x41

static unsigned char serial_buf[UART1_PUTCHAR_BUF_SIZE];
static unsigned char serial_buf_index = 0;

/*---------------------------------------------------------------------------*/
static u16_t
crc_byte(u16_t crc, u8_t b)
{
  crc = (u8_t)(crc >> 8) | (crc << 8);
  crc ^= b;
  crc ^= (u8_t)(crc & 0xff) >> 4;
  crc ^= crc << 12;
  crc ^= (crc & 0xff) << 5;
  return crc;
}
/*---------------------------------------------------------------------------*/
static uint16_t
writeb_crc(unsigned char c, uint16_t crc)
{
  /* Escape bytes:
        7d -> 7d 5d
        7e -> 7d 5e */
  if(c == ESCAPE_BYTE){
    uart1_writeb(ESCAPE_BYTE);
    uart1_writeb(0x5d);
  } else if(c == SYNCH_BYTE){
    uart1_writeb(ESCAPE_BYTE);
    uart1_writeb(0x5e);
  } else {
    uart1_writeb(c);
  }

  return crc_byte(crc, c);
}
/*---------------------------------------------------------------------------*/
int
putchar(int c)
{
  int i;
  uint16_t crc;
  char ch = ((char) c);

  /* Buffer outgoing character until newline, needed to determine the payload
   * length.
   */
  if(serial_buf_index < UART1_PUTCHAR_BUF_SIZE) {
    serial_buf[serial_buf_index++] = ch;
  }
  if(serial_buf_index < UART1_PUTCHAR_BUF_SIZE && ch != '\n') {
    return c;
  }

  /* Packetize and write buffered characters to serial port */

  /* Start of frame */
  crc = 0;
  uart1_writeb(SYNCH_BYTE);

  /* Protocol (noack) */
  crc = writeb_crc(PROT_NOACK, crc);

  /* Sequence */
  crc = writeb_crc(0x00, crc);

  /* Destination */
  crc = writeb_crc(0xFF, crc);
  crc = writeb_crc(0xFF, crc);

  /* Source */
  crc = writeb_crc(0x00, crc);
  crc = writeb_crc(0x00, crc);

  /* Payload length = full buffer size */
  crc = writeb_crc(UART1_PUTCHAR_BUF_SIZE, crc);

  /* Group */
  crc = writeb_crc(0x00, crc);

  /* Message ID */
  crc = writeb_crc(MSG_ID, crc);

  /* Actual payload characters */
  for (i=0; i < serial_buf_index; i++) {
    crc = writeb_crc(serial_buf[i], crc);
  }

  /* Pad with zeros up to full buffer size */
  for (i=serial_buf_index; i < UART1_PUTCHAR_BUF_SIZE; i++) {
  	crc = writeb_crc(0x00, crc);
  }

  /* CRC */
  /* Note: calculating but ignoring CRC for these two... */
  writeb_crc((uint8_t) (crc & 0xFF), 0);
  writeb_crc((uint8_t) ((crc >> 8) & 0xFF), 0);

  /* End of frame */
  uart1_writeb(SYNCH_BYTE);
  serial_buf_index = 0;

  return c;
}
#else /* TINYOS_SERIAL_FRAMES */
int
putchar(int c)
{
  uart1_writeb((char)c);
  return c;
}
#endif /* TINYOS_SERIAL_FRAMES */
