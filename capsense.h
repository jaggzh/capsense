#ifndef _CAPSENSE_H
#define _CAPSENSE_H

#include "ringbuffer.h"
#include "capproc.h"

#define CP_DEBUG 0
/* #define CP_DEBUG 2 */

#define CP_DTYPE float
#define BST_UNKNOWN         0  // button states
#define BST_OPEN            1
#define BST_OPEN_DEBOUNCE   2
#define BST_CLOSED          3
#define BST_CLOSED_DEBOUNCE 4

// 71677 492 374.51 364.69 363.71 363.31 363.52
// 71677 316 359.89 361.65 362.22 362.57 363.15
// 71690 310 347.41 358.42 360.59 361.75 362.74
#define CP_LINEBUFSIZE 1000
#define CP_COL_DATASTART 1   // don't want to normalize/scale off the growing ms column
#define COL_MS_I       0
#define COL_RAW_I      1
#define COL_VDIV4_I    2
#define COL_VDIV8_I    3
#define COL_VDIV16_I   4
#define COL_VDIV32_I   5
#define COL_VDIV64_I   6
#define COL_VDIV128_I  7
#define COL_VDIV128a_I 8
#define COL_VDIV256_I  9
#define COL_VDIFF_I    10
#define CP_COLCNT   11

// Column count in the data log files
// You might need to adjust this depending on how much you're storing there
// For instance, at the time of this generating the /256 (COL_VDIV256_I) column
//  from the raw value, not reading it from the file.
// Also, the 128 values are in the file, but I'm re-creating them anyway
//  (because I left that code there)
#define DATA_FILE_COLS 8

struct capsense_st {
	CP_DTYPE cols[CP_COLCNT];
	struct ringbuffer_st rb_range_real;
	struct ringbuffer_st *rb_range;
	CP_DTYPE mn, mx;
	float smoothmin, smoothmax; // recent min and max (from buffered)
	char init;
	int bst;
	float sensitivity;
	void (*cb_press)(struct capsense_st *cp);
	void (*cb_release)(struct capsense_st *cp);
	unsigned long safety_time_start_ms; // 0 is safety not enabled right now
};
typedef struct capsense_st cp_st;

#define CAP_DUMP_DATA
//#define CAPPROC_SERIAL_DEBUG // define if you want serial debug output
//#define CAPPROC_SERIAL_INIT  // define if you want serial debug output and want us to init it
/* Info:
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


void ringbuffer_minmax(cp_st *cp, RB_DTYPE mn, RB_DTYPE mx);
void capsense_proc(cp_st *cp);
void capsense_procstr(cp_st *cp, char *buf);
void cp_set_cb_press(cp_st *cp, void (*cb)(cp_st *cp));
void cp_set_cb_release(cp_st *cp, void (*cb)(cp_st *cp));
void cp_set_sensitivity(cp_st *cp, float newval);

void update_smoothed_limits(cp_st *cp);
void update_diff(cp_st *cp);
void detect_pressevents(cp_st *cp);
cp_st *capnew(void);
void _cap_init(cp_st *cp);

// loop_cap(): Call this for the lib to update, receiving
// sensor data (currently from Serial2)
void loop_cap(cp_st *cp, unsigned long now); // now: pass current millis()

// The below is included always in case
#ifndef _IN_CAPSENSE_C
	/* #ifdef _IN_CAPSENSE_TEST_C */
	/* 	#warning In cap we r externing colchars fine */
	/* #endif */
	extern char colchars[CP_COLCNT];
	extern char *colnames[CP_COLCNT];
	extern char *colfgs[CP_COLCNT];
	extern char cp_sense_debug_data;
#endif // /_IN_CAPSENSE_C


#endif // /_CAPSENSE_H
