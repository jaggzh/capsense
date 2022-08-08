#ifndef _CAPPROC_H
#define _CAPPROC_H

#include "capsense.h"

#ifdef __cplusplus
extern "C" {
#endif

//#define CAPPROC_SERIAL_DEBUG // define if you want serial debug output
//#define CAPPROC_SERIAL_INIT  // define if you want serial debug output and want us to init it


/*
Info:
   AVR: A0 = Transmit to ESP <-- this is my separate sensor -- the AVR pin there
 ESP32: UART RX: Using u2rxd as our local RX pin
*/

#define TXPIN 17 /* u2txd. TX. Dummy pin. We're not tx'ing */
#define RXPIN 16 /* u2rxd. RX. */
#define CHUNKSIZE 4  // Receiving 4 bytes at a time over serial
#define SENSOR_BAUD 57600

/* Delay for reducing sensor serial reads.
 * seconds / ( (bits/second) / (bits/byte)) = seconds*2 / byte?
 * I don't know what I'm doing there bit it made sense at the time,
 * and it comes out to:
 * (5 seconds / 6400) = .78ms delay between read attempts.
 *
 * The delay is based on the baud rate (ie. max transfer rate)
 * But ultimately our cap sensor is currently sending much slower.
 *
 * (It's in ms, so we *1000). We multiply x5 to try more frequently to
 * make sure we don't miss anything.
 * (/9 is for 9 bits per byte (8n1)) */

#define SENSOR_DELAY_MS ((int)((1000*5) / (SENSOR_BAUD/9)))

void setup_cap();
void loop_cap(unsigned long now); // now: pass current millis()

#ifdef __cplusplus
}
#endif

#endif // /_CAPPROC_H

