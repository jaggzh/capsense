
#include <HardwareSerial.h>
#include <PrintHero.h>
#include <Arduino.h>

#include "capproc.h"

#define MAGICCHUNK_DEBUG
#include "MagicSerialDechunk.h"

struct SerialDechunk dechunk_real;
struct SerialDechunk *dechunk = &dechunk_real;

uint16_t avg_div=4;

void procval(uint16_t v) {
	static float avg_short=v;
	static float avg8=v;
	static float avg16=v;
	static float avg32=v;
	static float avg64=v;
	static float avg128=v;
	static float avg128a=v; // Assymetric (drops faster)
	avg_short += (v-avg_short)/avg_div;
	avg8 += (v-avg8)/8;
	avg16 += (v-avg16)/16;
	avg32 += (v-avg32)/32;
	avg64 += (v-avg64)/64;
	avg128 += (v-avg128)/128;
	if (v-avg128a > 0) {  // Assymetric avg (drops faster than rises)
		avg128a += (v-avg128a)/128;
	} else {
		avg128a += (v-avg128a)/32;
	}
	DSP(millis());
	DSP(' ');
	DSP(v);
	DSP(' ');
	DSP(avg_short);
	DSP(' ');
	DSP(avg8);
	DSP(' ');
	DSP(avg16);
	DSP(' ');
	DSP(avg32);
	DSP(' ');
	DSP(avg64);
	DSP(' ');
	DSP(avg128);
	DSP(' ');
	DSP(avg128a); // assymetric
	DSP('\n');
	capsense_proc(avg_short, avg8, avg16, avg32, avg64, avg128, avg128a);
}

void dechunk_cb(struct SerialDechunk *sp) {
	/* DSP('{'); */
	uint16_t val;
	for (unsigned int i=0; i<sp->chunksize; i+=2) { // +=2 for 16 bit
		/* printf("%d ", sp->b[i]); */
		val = sp->b[i] | sp->b[i+1]<<8;
		/* DSPL(val); */
		procval(val);
		/* DSPL(sp->b[i]); */
		/* DSP(' '); */
	}
	/* DSPL('}'); */
}

#if CAPPROC_SERIAL_DEBUG
void loop_local_serial() {
	uint16_t newv;
	bool updated=false;
	if (Serial.available()) {
		int c = Serial.read();
		if (c == 'h') {
			newv = avg_div - avg_div / 2;
			if (newv == avg_div) newv--;
			if (newv == 0)       newv=1;
		} else if (c == 'l') {
			newv = avg_div + avg_div / 2;
			if (newv == avg_div) newv++;
			/* don't mind overflow/wrapping */
		} else {
			return;
		}
		sp("Changing avg divisor from ");
		sp(avg_div);
		sp(" to ");
		spl(newv);
		avg_div = newv;
		delay(500); // some time to see note
	}
}
#endif

void setup_cap() {
	#ifdef CAPPROC_SERIAL_INIT
		Serial.begin(115200);
	#endif
	Serial2.begin(SENSOR_BAUD, SERIAL_8N1, RXPIN, TXPIN);
	serial_dechunk_init(dechunk, CHUNKSIZE, dechunk_cb);
}

void loop_cap(unsigned long now) { // now: pass current millis()
	/* static uint8_t i=0; */
	/* i++; */
	/* static int wrap=0; */
	/* unsigned long now=millis(); */
	static unsigned long last_sensor_serial_check=now;
	#if CAPPROC_SERIAL_DEBUG
		static unsigned long last_local_serial_check=now;
		if (now - last_local_serial_check > SENSOR_DELAY_MS) {
			last_local_serial_check = now;
			loop_local_serial();
		}
	#endif
	if (now - last_sensor_serial_check > SENSOR_DELAY_MS) {
		last_sensor_serial_check = now;
		if (Serial2.available()) {
			uint8_t c = Serial2.read();
			/* DSP(c); */
			/* ++wrap; */
			/* if (wrap == 8) DSP("  "); */
			/* else if (wrap >= 16) { wrap=0; DSP('\n'); } */
			/* else DSP(' '); */
			dechunk->add(dechunk, c);
		} else {
			/* DSP("No ser.available()\n"); */
		}
	}
}

