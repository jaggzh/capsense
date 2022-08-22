#define _IN_CAPSENSE_C

#if defined(ARDUINO) || defined(ESP_PLATFORM)
	#include <Arduino.h>
	#include <HardwareSerial.h>
#endif

#include <limits.h>  // INT_MIN/MAX
#include <stdlib.h>  // strtof()
#include <errno.h>
#include <stdio.h>	// printf
#include <math.h>	// printf

#define MAGICCHUNK_DEBUG
#include <MagicSerialDechunk.h>
#include <PrintHero.h>
#include "testdefs.h"
#include "ringbuffer.h"
#include "tests/millis.h"
#ifdef CAPTEST_MODE
#include "tests/bansi.h"
#endif
#include "capsense.h"

#if 0 // esp32 build doesn't define ARDUINO. Bleh.
#ifndef ARDUINO
#include "tests/millis.h"
#include "tests/bansi.h"
#endif
#endif // 0

#define pf printf

/* Handle capacitive sensor samples */
// 164 samples/sec currently
int sps = 164;
int debounce_samples = 15;	// ~ 30ms
// debounce_wait_time = debounce_samples * (1/164);
// .0060975609s / sample

float close_mult_diff = .03; // Fraction of noise for differential comparison
float open_mult_diff = .02;

/*
 Gripping/movement settings
 grip_eval_delay_ms
 When gripping from a distance, capacitance will rise a lot more than
 a normal grip.
 This seems to take from 1 to 2s for a somewhat relaxed pace of hand
 placement.
 */
int grip_eval_checkpoint_ms = 60;
int grip_eval_delay_samples;	 // Set in _cap_init()
unsigned long chaos_safety_time_ms = 1000; // datetime.timedelta(milliseconds=1000);

int cmillis = 0;
int press_start_i = -1;
int press_start_ms = -1;
float press_start_y = 0;
float max_since_press = -1;
uint16_t avg_div=4;
#if CP_DEBUG_DATA > 0
	char cp_sense_debug_data=1;
	char cp_sense_debug=0;
#else
	char cp_sense_debug_data=0;
	char cp_sense_debug=0;
#endif

struct SerialDechunk dechunk_real;
struct SerialDechunk *dechunk = &dechunk_real;

void cp_set_cb_press(cp_st *cp, void (*cb)(struct capsense_st *cp)) {
	cp->cb_press = cb;
}
void cp_set_cb_release(cp_st *cp, void (*cb)(struct capsense_st *cp)) {
	cp->cb_release = cb;
}
void cp_set_sensitivity(cp_st *cp, float newval) {
	cp->sensitivity = newval;
}

const char *colnames[CP_COLCNT] = {
	"vdiv4", "vdiv8", "vdiv16",
	"vdiv32", "vdiv64", "vdiv128", "vdiv128a",
	"vdiv256", "slow", "vdiff"
};
char colchars[CP_COLCNT] = {
	0, 0, 0,
	'6', '7', '8', 'a',
	0, '|', 'x'
};
const char *rawfg = "\033[38;2;128;128;128m"; 
const char *colfgs[CP_COLCNT] = {
	0, 0, "\033[38;2;255;255;0m",
	"\033[38;2;255;0;255m", "\033[38;2;0;127;127m", 0, "\033[38;2;0;255;255m",
	0, "\033[38;2;255;155;155m", "\033[48;2;0;0;255m\033[38;2;255;255;255m"
};
int lookbehind=32;

void _cp_ringbuffer_set_minmax(cp_st *cp) {
	rb_st *rb = cp->rb_range;
	float lmn=(float)INT_MAX;
	float lmx=(float)INT_MIN;
	CP_DTYPE v;
	for (int i=0; i<rb->sz; i++) {
		v = rb->d[i];
		if (lmn > v) lmn = v;
		if (lmx < v) lmx = v;
	}
	cp->mn = lmn*0.999;
	cp->mx = lmx*1.001;
	// avgs.append(sum(d['raw'][st:en]) / (en-st))
}

void set_safety_timeout(cp_st *cp, int ms, int y, int yref) {
	cp->safety_time_start_ms = ms;
	/* plot.axvline(ms, color='magenta') */
	/* if y is not None and yref is not None: */
	/*	   plot.vlines(x=[ms], ymin=yref, ymax=y, color='blue', lw=4) */
}

void mark_safety_end(cp_st *cp, int ms) {
	/* global safety_time_start_ms */
	/* plot.axvline(ms, color='brown') */
	cp->safety_time_start_ms = 0;
}

// fail_safety_points(): Return 1 if fails test (should open button)
// ..Failure means it locks for the safety time period
char fail_safety_points(cp_st *cp, int i, int ms, int y,
		float noiserange, float chaosdelta,
		int chaosy, int chaosyref, int curd) {
	return 0;
	if (i - press_start_i > grip_eval_delay_samples) {
		if (y > curd + noiserange*2) {
			cp->safety_time_start_ms = ms;
			/* mark_safety_fail(plot=plot, ms=d['millis'][i], y=y) */
			cp->bst = BST_OPEN;
			return 1;
		}
	}
	// if chaosdelta < noiserange*.8:  # Don't let crazy time trigger
	//	   bst = 'open'
	//	   set_safety_timeout(plot=plot, d=d, ms=d['millis'][i], y=chaosy, yref=chaosyref)
	return 0;
}

void trigger_press(cp_st *cp) {
	#if CP_DEBUG_TERM > 1
		spal("CP: PRESS EVENT");
	#endif
	/* spal((int)cp->cb_press); */
	if (cp->cb_press) (*cp->cb_press)(cp);
}
void trigger_release(cp_st *cp) {
	#if CP_DEBUG_TERM > 1
		spal("CP: RELEASE EVENT");
	#endif
	/* spal((int)cp->cb_release); */
	if (cp->cb_release) (*cp->cb_release)(cp);
}

char reading_is_open(float cval, float press_start_y,
	                 float max_since_press, float open_drop_fraction) {
    float height = max_since_press - press_start_y;
    if (cval < max_since_press - height*open_drop_fraction)
    	return 1;
    return 0;
}

const char diff_debug_fmt[]="diff%s: diff=%7.4f > %c(noise=%6.3f * mult=%2.2f * sensi=%3.3f)=%s%7.4f%s (clo=%4d ope=%4d)\n";
char stg_show_closed=1;
char stg_show_open=1;


#define DIFF_RES_OPEN   -1
#define DIFF_RES_CLOSED  1
#define DIFF_RES_NOTHING 0

char diff_results_in(cp_st *cp, float diff, float noiserange, float close_mult_diff) {
	// returns 1 if closed. 0 for nothing noticable. -1 for open
	signed char rc=DIFF_RES_NOTHING;
	static int ctr_closed=0;
	static int ctr_open=0;
	static long int lmillis=0;
	long int now=millis();
	const int debug_output_ms = 50; // time between

	float thresh = noiserange * close_mult_diff * cp->sensitivity;

    if (diff > thresh) ctr_closed += 5;
	else if (ctr_closed>0) ctr_closed -= 4;

    if (diff < -thresh*.75) ctr_open += 5;
	else if (ctr_open>0) ctr_open -= 4;

    if (ctr_open > 40)   { rc=DIFF_RES_OPEN;   ctr_open -= 5;   }
    if (ctr_closed > 40) { rc=DIFF_RES_CLOSED; ctr_closed -= 5; }

	#if CP_DEBUG_TERM > 1 || CP_DEBUG_SERIAL > 0
		if (  (1 || rc) &&
			  (stg_show_closed || stg_show_open) &&
			  (int)(now-lmillis) > debug_output_ms) {
			lmillis = millis();
			char buf[1000];
			const char *statemsg, *statespacer1, *statespacer2;
			if (rc==DIFF_RES_CLOSED) {
				statemsg="{CLOSED}";
				statespacer1="";
				statespacer2="       ";
			} else if (rc==DIFF_RES_OPEN) {
				statemsg="{OPEN}  ";
				statespacer1="       ";
				statespacer2="";
			} else {
				statemsg="--------";
				statespacer1 = "-------";
				statespacer2 = "";
			}
			if (1 ||
				  (rc == DIFF_RES_CLOSED && stg_show_closed) || 
			      (rc == DIFF_RES_OPEN   && stg_show_open)) {
				sprintf(buf, diff_debug_fmt,
					statemsg,
					diff, ' ', noiserange, close_mult_diff, cp->sensitivity,
					statespacer1,
					noiserange * close_mult_diff * cp->sensitivity,
					statespacer2,
					ctr_closed, ctr_open);
				#if CP_DEBUG_SERIAL > 0
					sp(buf);
				#else 
					fputf(buf, stdout);
				#endif
			}
		}
	#endif
    return rc;
}

void _detect_pressevents(cp_st *cp) {
	/* global press_start_i */
	/* global press_start_ms */
	/* global safety_time_start_ms */
	/*	Noiserange: Based on recent max/mins of signal (vdiv4) */
	/*			 This lets us pick a multiplier for a threshold */
	/*			 to know what to consider a press or release */
	/*			 (ie. 2x above slow_average (dst128) for press, */
	/*			 and 1.5x for release) */
	float smoothmin = cp->smoothmin;
	float smoothmax = cp->smoothmax;
	float noiserange = smoothmax - smoothmin;
	float starty = smoothmin;
	CP_DTYPE *cols = cp->cols;
	float startyref = cols[COL_VDIV128_I];
	float startdelta = starty - startyref;
	float endy = smoothmin;
	float endyref = cols[COL_VDIV128_I];
	float enddelta = endyref - endy;
	float chaosy = smoothmin;
	float chaosyref = cols[COL_VDIV32_I];
	float chaosdelta = chaosy - chaosyref;
	unsigned long ms = cp->ms;
	float open_tracking_value = cols[COL_VDIV64_I];
	/* float open_drop_fraction = .7; */
	float close_thresh = .8;
	float open_thresh = .5;
	float diff = cp->cols[COL_VDIFF_I];
	char diff_result;

	static int st=0, en=0;
	//static int stref = 0;
	static unsigned long sample_count = -1;

	sample_count++;
	if (cp->safety_time_start_ms) {
		unsigned long dist = ms - cp->safety_time_start_ms;
		#if CP_DEBUG_TERM > 1
			printf("Safety: %lu\n", cp->safety_time_start_ms);
			printf("Millis: %lu\n", ms);
			printf("Millis-Safety: %lu\n", dist);
		#endif
		if (dist > chaos_safety_time_ms) {
				/* # mark_safety_end(plot=plot, d=d, ms=cp->safety_time_start_ms) */
			cp->safety_time_start_ms = 0;
		} else {
			return; // we're within safety shutoff still (I think)
		}
	}

	/* # If safety timeout was set and it's too soon, quit */
	/* # if cp->safety_time_start_ms is not None: */
	/* #	 if d['millis'][i] - cp->safety_time_start_ms < 2000: return */
	#if CP_DEBUG_TERM > 1
		printf(BBLA "Cols:");
		for (int i=1; i<CP_COLCNT; i++) printf(" %.2f", cp->cols[i]);
		printf(WHI " [Min %.2f Max %.2f]", smoothmin, smoothmax);
		printf(RST "\n");
	#endif

	diff_result = diff_results_in(cp, diff, noiserange, close_mult_diff);

	cp->closed_set = 0;
	cp->open_set = 0;

	if (cp->bst == BST_OPEN) { // Default state: Open == not a cap-sense press
		#if CP_DEBUG_TERM > 1
			pf(BCYA "BST: BST_OPEN\n" RST);
			pf(" Startdelta=%f > (noiserange=%f * closethresh=%f = %f)\n",
				startdelta, noiserange, close_thresh, noiserange*close_thresh);
		#endif

		/* if (startdelta > noiserange*close_thresh) { */
		if (diff_result == DIFF_RES_CLOSED) {
			#if CP_DEBUG_TERM > 1
				printf(YEL " -> CLOSED DEBOUNCE\n" RST);
			#endif
			st = sample_count;
			//stref = startyref;
			cp->bst = BST_CLOSED_DEBOUNCE;
			//mark_start_try(plot=plot, ms=d['millis'][i], y=starty);
		}
	} else if (cp->bst == BST_CLOSED_DEBOUNCE) {
		#if CP_DEBUG_TERM > 1
			pf(BCYA "BST: BST_CLOSED_DEBOUNCE\n" RST);
			pf(" (sample_count=%lu-st=%d)=%lu > debounce_samples=%d\n",
				sample_count, st, sample_count-st, debounce_samples);
		#endif
		if ((int)sample_count-st > debounce_samples) {
			#if CP_DEBUG_TERM > 1
				pf("  Startdelta=%f > (noiserange=%f * closethresh=%f = %f)\n",
					startdelta, noiserange, close_thresh, noiserange*close_thresh);
			#endif
			/* if (startdelta > noiserange*close_thresh) { */
			if (diff_result == DIFF_RES_CLOSED) {
				#if CP_DEBUG_TERM > 1
					printf(YEL "  -> CLOSED\n" RST);
				#endif
				cp->bst = BST_CLOSED;
				cp->closed_set = 1;
				press_start_i = sample_count;
				press_start_ms = ms;
				press_start_y = cols[COL_VDIV16_I];
				// ***** RESETTING AN AVERAGE HERE \/ \/
				// cp->cols[COL_VDIV256_I] = open_tracking_value;
				//mark_start_true(plot=plot, ms=d['millis'][i], y=starty);
				trigger_press(cp);
			} else {
				#if CP_DEBUG_TERM > 1
					printf(YEL " -> OPEN\n" RST);
				#endif
				cp->bst = BST_OPEN;
				/* mark_start_abort(plot=plot, ms=d['millis'][i], y=starty) */
			}
		}
	} else if (cp->bst == BST_CLOSED) {
			//        __ _t
			//   /   /  \    \ h*tolerance
			//  h   /    \__ / ___
			//   \ /      \    \  considered open 
		#if CP_DEBUG_TERM > 1
			pf(BCYA "BST: BST_CLOSED\n" RST);
			pf(" fail_safety_points()\n");
			pf("   future: else if (enddelta=%f > (noiserange=%f * openthresh=%f = %f)\n",
				enddelta, noiserange, open_thresh, noiserange*open_thresh);
		#endif

		// Raising detected peak of press values
		if (max_since_press < open_tracking_value) max_since_press = open_tracking_value;
		/* char isopen = reading_is_open(open_tracking_value, press_start_y, */
		/* 			max_since_press, open_drop_fraction); */

		if (fail_safety_points(cp, sample_count, ms, starty,
				noiserange,
				chaosdelta, chaosy, chaosyref,
				press_start_y)) {
			// Tests and issues mark_safety_fail() if needed to plot
			#if CP_DEBUG_TERM > 1
				printf(YEL "  -> CLOSED\n" RST);
			#endif
			cp->bst = BST_OPEN;
		//} else if (isopen || enddelta > noiserange*open_thresh) {
		} else if (diff_result == DIFF_RES_CLOSED  || enddelta > noiserange*open_thresh) {
			#if CP_DEBUG_TERM > 1
				printf(YEL " -> OPEN_DEBOUNCE\n" RST);
			#endif
			en = sample_count;
			// mark_end_try(plot=plot, ms=d['millis'][i], y=endy);
			cp->bst = BST_OPEN_DEBOUNCE;
		}
	} else if (cp->bst == BST_OPEN_DEBOUNCE) {
		#if CP_DEBUG_TERM > 1
			pf(BCYA "BST: BST_OPEN_DEBOUNCE\n" RST);
		#endif
		if ((int)(sample_count-en) > debounce_samples) {
			/* char isopen = reading_is_open(open_tracking_value, press_start_y, */
			/* 			max_since_press, open_drop_fraction); */
			// if (enddelta > noiserange*open_thresh) {
			/* if (isopen) { */
			if (diff_result == DIFF_RES_CLOSED) {
				#if CP_DEBUG_TERM > 1
					printf(YEL " -> OPEN\n" RST);
				#endif
				cp->open_set = 1;
				cp->bst = BST_OPEN;
				/* mark_end_true(plot=plot, ms=d['millis'][i], y=endy) */
				/* mark_pressrange(d['millis'][st], d['millis'][i], d=d, plot=plot) */
				trigger_release(cp);
			} else {
				#if CP_DEBUG_TERM > 1
					printf(YEL " -> CLOSED\n" RST);
				#endif
				cp->bst = BST_CLOSED;
				/* mark_end_false(plot=plot, ms=d['millis'][i], y=endy) */
				/* plot.vlines(x=[ms], ymin=endyref, ymax=endy, color='red', lw=4) */
			}
		}
	}
}

void capsense_procstr(cp_st *cp, char *buf) {
	static unsigned long lineno = 0;
	char *bufp = buf;
	char *nbufp;

	lineno++;

	int i=0;
	#if CP_DEBUG_TERM > 1
		fprintf(stderr, "In [%lu]: %s", lineno, buf);
	#endif
	// for (; i<CP_COLCNT; i++)
		// 273179 275
		// 273192 501
		// 273192 497
	// We switched to only taking "{ms} {value}".
	// This loop used to take a bunch of pre-computed moving averages
	errno = 0;
	// No longer using ms either
	/* cp->ms = strtol(bufp, &nbufp, 10); */
	/* if (errno == ERANGE) { */
	/* } else { */
		/* bufp = nbufp; */
		/* printf("Str: %s\n", buf); */
		cp->raw = atoi(bufp);
		/* printf("Raw: %d\n", cp->raw); */
	/* } */
	if (errno == ERANGE) {
		#if CP_DEBUG_TERM > 1
			fprintf(stderr, "Error in line %lu, col %d, value %f\n", lineno, i, cp->cols[i]);
		#elif CP_DEBUG_SERIAL > 0
			DSP(F("Error in serial data: "));
			DSPL(buf);
		#endif
		return;
	}
	capsense_proc(cp, cp->ms, cp->raw);
}

void _capsense_print_data(cp_st *cp) {
	#ifdef CAP_DUMP_DATA
	static int slowctr=0;
	if (slowctr++ > 4) slowctr=0;
	slowctr=0;
	if (cp_sense_debug_data && !slowctr) {
		/* DSP(cp->ms); */
		/* DSP(' '); */
		/* DSP("Raw:"); */
		DSP(cp->raw);
		/* DSP(' '); */
		/* DSP(avg_short); */
		/* DSP(' '); */
		/* DSP(avg8); */
		/* DSP(' '); */
		/* DSP(avg16); */
		/* DSP(' '); */
		/* DSP("32:");        // keep */
		/* DSP(cp->cols[COL_VDIV32_I]);        // keep */
		/* DSP(" Raw:"); */
		/* DSP(v); */
		/* DSP(" 64:"); */
		/* DSP(avg64); */
		/* DSP(" 128:"); */
		/* DSP(avg128); */
		/* DSP(" 128a:"); */
		/* DSP(avg128a); // assymetric */
		/* DSP(" slowest:"); */
		/* DSP(avgslowest); */
		/* DSP("smin:"); */
		/* DSP(cp->smoothmin); */
		#if 0
			DSP(" Base:");
			DSP(VDIFF_PLOT_LOC_BASE);
			DSP(" Sens:");
			DSP(VDIFF_PLOT_LOC_BASE + cp->sensitivity);
			DSP(" Diff:");
			DSP(VDIFF_PLOT_YLOC());
			DSP(" Closed:");
			
			float noiserange = cp->smoothmax - cp->smoothmin;
			/* if (diff > noiserange * close_mult_diff * cp->sensitivity) return 1; */
			// #define VDIFF_PLOT_YLOC() (400 + cp->cols[COL_VDIFF_I]*VDIFF_YSCALE)
			DSP(cp->closed_set ?
					(VDIFF_PLOT_LOC_BASE + cp->cols[COL_VDIFF_I] + noiserange * close_mult_diff * cp->sensitivity) :
					VDIFF_PLOT_YLOC());
			/* DSP(cp->closed_set ? VDIFF_PLOT_YLOC()+5 : VDIFF_PLOT_YLOC()); */
			DSP(" Open:");
			DSP(cp->open_set ?
					(VDIFF_PLOT_LOC_BASE + cp->cols[COL_VDIFF_I] - noiserange * close_mult_diff * cp->sensitivity) :
					VDIFF_PLOT_YLOC());

			/* DSP(cp->open_set ? VDIFF_PLOT_YLOC()-5 : VDIFF_PLOT_YLOC()); */
		#endif
		DSP('\n');
	}
	#endif
}

void _gen_val_avgs(cp_st *cp, unsigned long now, uint16_t v) {
	/* avg_short += (v-avg_short)/avg_div; */
	cp->cols[COL_VDIV4_I]   += (v - cp->cols[COL_VDIV4_I])/4;
	cp->cols[COL_VDIV8_I]   += (v - cp->cols[COL_VDIV8_I])/8;
	cp->cols[COL_VDIV16_I]  += (v - cp->cols[COL_VDIV16_I])/16;

	/* cp->cols[COL_VDIV32_I]  += (v - cp->cols[COL_VDIV32_I])/32; */

	/* static float residual32=0; */
	/* float newval = cp->cols[COL_VDIV32_I]; // cols = moving averages stored here */
	/* float contribution = (v-newval + residual32) * (1.0f/32.0f); */
	/* residual32 = newval - v + residual32 - contribution * 32; */
	/* printf("Diff made: Newval=%f, cont=%f, resid32=%f\n", newval, contribution, residual32); */
	/* cp->cols[COL_VDIV32_I] += contribution; */


	cp->cols[COL_VDIV32_I]  += (v - cp->cols[COL_VDIV32_I])/32;
	cp->cols[COL_VDIV64_I]  += (v - cp->cols[COL_VDIV64_I])/64;
	cp->cols[COL_VDIV128_I] += (v - cp->cols[COL_VDIV128_I])/128;
	cp->cols[COL_VDIV256_I] += (v - cp->cols[COL_VDIV256_I])/256;
	cp->cols[COL_VDIVSLOWEST_I] += (v - cp->cols[COL_VDIVSLOWEST_I])/4000;
	if (v - cp->cols[COL_VDIV128a_I] > 0) {  // Assymetric avg (drops faster than rises)
		cp->cols[COL_VDIV128a_I] += (v - cp->cols[COL_VDIV128a_I])/128;
	} else {
		cp->cols[COL_VDIV128a_I] += (v - cp->cols[COL_VDIV128a_I])/32;
	}
	// ^  we set COL_VDIFF_I below
}

void _dechunk_cb(struct SerialDechunk *sp, void *userdata) {
	/* DSP('{'); */
	uint16_t val;
	for (unsigned int i=0; i<sp->chunksize; i+=2) { // +=2 for 16 bit
		/* printf("%d ", sp->b[i]); */
		val = sp->b[i] | sp->b[i+1]<<8;
		/* DSPL(val); */
		capsense_proc((cp_st *)userdata, millis(), val);
		/* DSPL(sp->b[i]); */
		/* DSP(' '); */
	}
	/* DSPL('}'); */
}

#ifdef CAPPROC_USER_SERIAL_CONTROLS
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
#endif // CAPPROC_USER_SERIAL_CONTROLS

cp_st *capnew(void) {
	cp_st *cp;
	cp = (cp_st *)calloc(1, sizeof(cp_st));
	if (!cp) {
		spl("Error malloc() cp_st struct");
		return 0;
	}
	_cap_init(cp);
	return cp;
}

void _cap_init(cp_st *cp) {
	#ifdef CAPPROC_SERIAL_INIT
		Serial.begin(115200);
	#endif

	cp->rb_range = &cp->rb_range_real;
	ringbuffer_init(cp->rb_range, lookbehind);
	ringbuffer_setall(cp->rb_range, 0);
	grip_eval_delay_samples = (int)(sps * grip_eval_checkpoint_ms * .001);
	if (!cmillis) cmillis = millis(); // Only do this once even with multiple caps!
	cp->smoothmin = INT_MAX;
	cp->smoothmax = -((float)INT_MAX);
	cp->bst = BST_OPEN;
	cp->sensitivity = 1.0;
	cp->safety_time_start_ms = 0;
	cp->thresh_diff = CP_THRESH_DIFF;
	cp->thresh_integ = CP_THRESH_INTEG;
	cp->leak_integ = CP_LEAK_INTEG_WHEN_OBJECT_DETECTED;
	cp->leak_integ_no = CP_LEAK_INTEG_WHEN_NOOBJECT_DETECTED;
	cp->integ = cp->integ_b4 = cp->diff = cp->diff_b4 = 0;

	#ifdef ESP_PLATFORM
		Serial2.begin(SENSOR_BAUD, SERIAL_8N1, RXPIN, TXPIN);
	#else
		// #error "We have no serial device to receive data"
	#endif
	/* #warning "capproc still has a global dechunk buffer and other global values. multiple cap sensors not usable." */
	// serial_dechunk_init takes a void* for the caller's own auxiliary data.
	// this will be passed to the callback.
	// we're using it to keep track of the sensor so multiple sensors can be
	// used.
	serial_dechunk_init(dechunk, CHUNKSIZE, _dechunk_cb, (void *)cp);
}

// Two serial processes are handled here:
// 1. Use for capsense library to handle Serial2 reading of values
//    which are added automatically to the MagicSerialDechunk
//    library object which processes the data as it receives it
// 2. User controls via Serial (see loop_local_serial())
void loop_cap_serial(cp_st *cp, unsigned long now) {
	// now: pass current millis()
	// newvalue: latest reading (because caller
	//           might get it from somewhere themselves)
	/* static uint8_t i=0; */
	/* i++; */
	/* static int wrap=0; */
	/* unsigned long now=millis(); */
	#ifdef CAPPROC_USER_SERIAL_CONTROLS
		static unsigned long last_local_serial_check=now;
		if (now - last_local_serial_check > SENSOR_DELAY_MS) {
			last_local_serial_check = now;
			loop_local_serial();
		}
	#endif
	#ifndef ESP_PLATFORM
		#warning "ESP_PLATFORM is not defined. We are NOT receiving serial data to the esp from a sensor"
	#else
		static unsigned long last_sensor_serial_check=now;
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
	#endif
}

#define REFVAL (cp->cols[COL_VDIV32_I])
/* #define REFVAL (cp->raw) */
void _update_diff(cp_st *cp) {
	/* printf("---------------\n"); */
	/* printf("REFVAL: %d\n", REFVAL); */
	/* printf("1 prior_basis: %u\n", cp->prior_basis); */
	float diff = ((float)REFVAL) - (float)cp->prior_basis;
	/* printf("REFVAL: %f\n", REFVAL); */
	/* printf("1 prior_basis: %f\n", cp->prior_basis); */
	/* printf("diff: %f\n", diff); */
	//cp->cols[COL_VDIFF_I] += (diff - cp->cols[COL_VDIFF_I]) / 32;
	cp->prior_basis = REFVAL;
	cp->diff = diff;

	float diffabs = fabsf(diff);

	/* if (diffabs > cp->thresh_diff && diffabs < CP_DIFF_CAP) { */
	if (diffabs > cp->thresh_diff) {
		cp->integ = cp->integ_b4 + diff;
	} else {
		cp->integ = cp->integ_b4;
	}

	if (cp->integ >= cp->thresh_integ) {
		cp->open_set = 0;
		cp->closed_set = 1;
		cp->bst = BST_CLOSED;
		trigger_press(cp);
		cp->integ_b4 = cp->integ * cp->leak_integ;
	} else {
		cp->open_set = 1;
		cp->closed_set = 0;
		cp->bst = BST_OPEN;
		trigger_release(cp);
		cp->integ_b4 = cp->integ * cp->leak_integ_no;
	}
	static int i=0;
	// Kinda working:
	// Diff:  -3.5004 Int:  33.7730 (dth:   0.010, ith:   1.000, il:   0.900)
	if (cp_sense_debug) {
		/* if (++i > 24) { // reduce output quantity with > # */
		if (++i > 0) { // reduce output quantity with > #
			i=0;
			char buf[150];
			sprintf(buf, "(%s) v:%5u Diff:%8.4f Int:%8.4f dth:%7.3f ith:%7.3f il:%7.3f ilno:%7.3f",
					cp->bst == BST_CLOSED ? "PRESSED" : "-open- ",
					cp->raw, diff, cp->integ, cp->thresh_diff, cp->thresh_integ,
					cp->leak_integ, cp->leak_integ_no);
			printf("%s\n", buf);
			fflush(stdout);
			/* Serial.println(buf); */
		}
	}
}

void capsense_proc(cp_st *cp, unsigned long now, uint16_t v) {
	/* # Make our own dividing moving average */
	/* def add_moving_div(d=None, div=None, label=None): */
	/*	   a = [d['raw'][0]] # first val */
	/*	   for i in range(1, len(d)): */
	/*		   newv = a[i-1] + (d['raw'][i] - a[i-1])/div */
	/*		   a.append(newv) */
	/*	   d[label]=a */
	/* add_moving_div(d=d, div=256, label='vdiv256') */

	cp->ms = now;
	cp->raw = v;
	if (!cp->init) {	  // first call we init on data, to suppress jumping vals
		ringbuffer_setall(cp->rb_range, v);
		cp->cols[COL_VDIFF_I] = 0;
		cp->prior_basis = cp->raw = v;
		for (int i=CP_COL_DATASTART; i<=CP_COL_AVGSLAST; i++) cp->cols[i] = v;
	} else {
		ringbuffer_add(cp->rb_range, v); // Used for moving min/max
	}
	_cp_ringbuffer_set_minmax(cp);
	/* DSP(" * "); */
	/* DSP(ms); DSP(' '); */
	/* DSP(v); DSP(' '); */

	_gen_val_avgs(cp, now, v); // also uses cp->init
	_update_smoothed_limits(cp);
	_update_diff(cp);
	/* _detect_pressevents(cp); */
	_capsense_print_data(cp);

	if (!cp->init) {
		cp->init=1;
	}
}

void _update_smoothed_limits(cp_st *cp) {
	//for (int i=CP_COL_DATASTART; i<CP_COLCNT; i++) {
	if (cp->smoothmin == (float)INT_MAX) cp->smoothmin = cp->mn;
	else cp->smoothmin += (cp->mn - cp->smoothmin)/6;

	if (cp->smoothmax == (float)INT_MIN) cp->smoothmax = cp->mx;
	else cp->smoothmax += (cp->mx - cp->smoothmax)/6;
	/* if (v < cp->smoothmin) cp->smoothmin = v; */
	/* if (v > cp->smoothmax) cp->smoothmax = v; */
}
	/* printf("Min:%f Max:%f\n", smoothmin, smoothmax); */
