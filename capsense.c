#define _IN_CAPSENSE_PROC_C
#include <ringbuffer/ringbuffer.h>
#include <limits.h>  // INT_MIN/MAX
#include <stdlib.h>  // strtof()
#include <errno.h>
#include <stdio.h>	// printf

#include "capsense.h"

#ifndef ARDUINO
#include "tests/millis.h"
#include "tests/bansi.h"
#endif

#define pf printf

/* Handle capacitive sensor samples */
// 164 samples/sec currently
int sps = 164;
int debounce_samples = 5;	// ~ 30ms
// debounce_wait_time = debounce_samples * (1/164);
// .0060975609s / sample

/*
 Gripping/movement settings
 grip_eval_delay_ms
 When gripping from a distance, capacitance will rise a lot more than
 a normal grip.
 This seems to take from 1 to 2s for a somewhat relaxed pace of hand
 placement.
 */
int grip_eval_checkpoint_ms = 60;
int grip_eval_delay_samples;	 // Set in capsense_init()
int chaos_safety_time_ms = 1000; // datetime.timedelta(milliseconds=1000);
unsigned long safety_time_start_ms = 0; // None

int cmillis = 0;
int press_start_i = -1;
int press_start_ms = -1;
float press_start_y = 0;

char *colnames[CP_COLCNT] = {
	"millis", "raw", "vdiv4", "vdiv8", "vdiv16",
	"vdiv32", "vdiv64", "vdiv128", "vdiv128a"
};
char colchars[CP_COLCNT] = {
	0, '*', 0, 0, '5',
	'6', '7', '8', 'a'
};
char *colfgs[CP_COLCNT] = {
	0, "\033[38;2;128;128;128m", 0, 0, "\033[38;2;255;255;0m",
	"\033[38;2;255;0;255m", "\033[38;2;0;127;127m", 0, "\033[38;2;0;255;255m"
};
int lookbehind=8;

void capsense_init(cp_st *cp) {
	cp->rb_range = &cp->rb_range_real;
	ringbuffer_init(cp->rb_range, lookbehind);
	ringbuffer_setall(cp->rb_range, 0);
	grip_eval_delay_samples = (int)(sps * grip_eval_checkpoint_ms * .001);
	if (!cmillis) cmillis = millis(); // Only do this once even with multiple caps!
	cp->smoothmin = INT_MAX;
	cp->smoothmax = -((float)INT_MAX);
	cp->bst = BST_OPEN;
}

void cp_ringbuffer_set_minmax(cp_st *cp) {
	rb_st *rb = cp->rb_range;
	float lmn=(float)INT_MAX;
	float lmx=(float)INT_MIN;
	CP_DTYPE v;
	for (int i=0; i<rb->sz; i++) {
		v = rb->d[i];
		if (lmn > v) lmn = v;
		if (lmx < v) lmx = v;
	}
	cp->mn = lmn;
	cp->mx = lmx;
	// avgs.append(sum(d['raw'][st:en]) / (en-st))
}

void update_smoothed_limits(cp_st *cp) {
	//for (int i=CP_COL_DATASTART; i<CP_COLCNT; i++) {
	if (cp->smoothmin == (float)INT_MAX) cp->smoothmin = cp->mn;
	else cp->smoothmin += (cp->mn - cp->smoothmin)/6;

	if (cp->smoothmax == (float)INT_MIN) cp->smoothmax = cp->mx;
	else cp->smoothmax += (cp->mx - cp->smoothmax)/6;
	/* if (v < cp->smoothmin) cp->smoothmin = v; */
	/* if (v > cp->smoothmax) cp->smoothmax = v; */
}
	/* printf("Min:%f Max:%f\n", smoothmin, smoothmax); */

void capsense_proc(cp_st *cp) {
	/* # Make our own dividing moving average */
	/* def add_moving_div(d=None, div=None, label=None): */
	/*	   a = [d['raw'][0]] # first val */
	/*	   for i in range(1, len(d)): */
	/*		   newv = a[i-1] + (d['raw'][i] - a[i-1])/div */
	/*		   a.append(newv) */
	/*	   d[label]=a */
	/* add_moving_div(d=d, div=256, label='vdiv256') */

	if (!cp->init) {	  // first call we init on data, to suppress jumping vals
		ringbuffer_setall(cp->rb_range, cp->cols[COL_VDIV16_I]);
		cp->init=1;
	}
	ringbuffer_add(cp->rb_range, cp->cols[COL_VDIV16_I]); // Used for moving min/max
	cp_ringbuffer_set_minmax(cp);
}

void set_safety_timeout(int ms, int y, int yref) {
	safety_time_start_ms = ms;
	/* plot.axvline(ms, color='magenta') */
	/* if y is not None and yref is not None: */
	/*	   plot.vlines(x=[ms], ymin=yref, ymax=y, color='blue', lw=4) */
}

void mark_safety_end(int ms) {
	/* global safety_time_start_ms */
	/* plot.axvline(ms, color='brown') */
	safety_time_start_ms = 0;
}

// fail_safety_points(): Return 1 if fails test (should open button)
// ..Failure means it locks for the safety time period
char fail_safety_points(cp_st *cp, int i, int ms, int y,
		float noiserange, float chaosdelta,
		int chaosy, int chaosyref, int curd) {
	if (i - press_start_i > grip_eval_delay_samples) {
		if (y > curd + noiserange*2) {
			safety_time_start_ms = ms;
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

void detect_pressevents(cp_st *cp) {
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
	unsigned long ms = cols[COL_MS_I];
	float close_thresh = .8;
	float open_thresh = .3;
	static int st=0, en=0;
	//static int stref = 0;
	static unsigned long sample_count=-1;

	sample_count++;
	if (safety_time_start_ms) {
		unsigned long dist = ms-safety_time_start_ms;
		#if CP_DEBUG > 1
			printf("Safety: %lu\n", safety_time_start_ms);
			printf("Millis: %lu\n", ms);
			printf("Millis-Safety: %lu\n", dist);
		#endif
		if (dist > chaos_safety_time_ms) {
				/* # mark_safety_end(plot=plot, d=d, ms=safety_time_start_ms) */
			safety_time_start_ms = 0;
		} else {
			return; // we're within safety shutoff still (I think)
		}
	}

	/* # If safety timeout was set and it's too soon, quit */
	/* # if safety_time_start_ms is not None: */
	/* #	 if d['millis'][i] - safety_time_start_ms < 2000: return */
	#if CP_DEBUG > 1
		printf(BBLA "Cols:");
		for (int i=1; i<CP_COLCNT; i++) printf(" %.2f", cp->cols[i]);
		printf(WHI " [Min %.2f Max %.2f]", smoothmin, smoothmax);
		printf(RST "\n");
	#endif

	if (cp->bst == BST_OPEN) { // Default state: Open == not a cap-sense press
		#if CP_DEBUG > 1
			pf(BCYA "BST: BST_OPEN\n" RST);
			pf(" Startdelta=%f > (noiserange=%f * closethresh=%f = %f)\n",
				startdelta, noiserange, close_thresh, noiserange*close_thresh);
		#endif

		if (startdelta > noiserange*close_thresh) {
			#if CP_DEBUG > 1
				printf(YEL " -> CLOSED DEBOUNCE\n" RST);
			#endif
			st = sample_count;
			//stref = startyref;
			cp->bst = BST_CLOSED_DEBOUNCE;
			//mark_start_try(plot=plot, ms=d['millis'][i], y=starty);
		}
	} else if (cp->bst == BST_CLOSED_DEBOUNCE) {
		#if CP_DEBUG > 1
			pf(BCYA "BST: BST_CLOSED_DEBOUNCE\n" RST);
			pf(" (sample_count=%lu-st=%d)=%lu > debounce_samples=%d\n",
				sample_count, st, sample_count-st, debounce_samples);
		#endif
		if (sample_count-st > debounce_samples) {
			#if CP_DEBUG > 1
				pf("  Startdelta=%f > (noiserange=%f * closethresh=%f = %f)\n",
					startdelta, noiserange, close_thresh, noiserange*close_thresh);
			#endif
			if (startdelta > noiserange*close_thresh) {
				#if CP_DEBUG > 1
					printf(YEL "  -> CLOSED\n" RST);
				#endif
				cp->bst = BST_CLOSED;
				press_start_i = sample_count;
				press_start_ms = ms;
				press_start_y = cols[COL_VDIV16_I];;
				//mark_start_true(plot=plot, ms=d['millis'][i], y=starty);
			} else {
				#if CP_DEBUG > 1
					printf(YEL " -> OPEN\n" RST);
				#endif
				cp->bst = BST_OPEN;
				/* mark_start_abort(plot=plot, ms=d['millis'][i], y=starty) */
			}
		}
	} else if (cp->bst == BST_CLOSED) {
		#if CP_DEBUG > 1
			pf(BCYA "BST: BST_CLOSED\n" RST);
			pf(" fail_safety_points()\n");
			pf("   future: else if (enddelta=%f > (noiserange=%f * openthresh=%f = %f)\n",
				enddelta, noiserange, open_thresh, noiserange*open_thresh);
		#endif
		if (fail_safety_points(cp, sample_count, ms, starty,
				noiserange,
				chaosdelta, chaosy, chaosyref,
				press_start_y)) {
			// Tests and issues mark_safety_fail() if needed to plot
			#if CP_DEBUG > 1
				printf(YEL "  -> CLOSED\n" RST);
			#endif
			cp->bst = BST_OPEN;
		} else if (enddelta > noiserange*open_thresh) {
			#if CP_DEBUG > 1
				printf(YEL " -> OPEN_DEBOUNCE\n" RST);
			#endif
			en = sample_count;
			// mark_end_try(plot=plot, ms=d['millis'][i], y=endy);
			cp->bst = BST_OPEN_DEBOUNCE;
		}
	} else if (cp->bst == BST_OPEN_DEBOUNCE) {
		#if CP_DEBUG > 1
			pf(BCYA "BST: BST_OPEN_DEBOUNCE\n" RST);
		#endif
		if (sample_count-en > debounce_samples) {
			if (enddelta > noiserange*open_thresh) {
				#if CP_DEBUG > 1
					printf(YEL " -> OPEN\n" RST);
				#endif
				cp->bst = BST_OPEN;
				/* mark_end_true(plot=plot, ms=d['millis'][i], y=endy) */
				/* mark_pressrange(d['millis'][st], d['millis'][i], d=d, plot=plot) */
			} else {
				#if CP_DEBUG > 1
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
	char *sp = buf;
	char *nsp;

	lineno++;

	int i=0;
	#if CP_DEBUG > 1
		fprintf(stderr, "In [%lu]: %s", lineno, buf);
	#endif
	for (; i<CP_COLCNT; i++) {
		errno = 0;
		cp->cols[i] = strtof(sp, &nsp);
		#if CP_DEBUG > 1
			fprintf(stderr, "Line %lu, col %d, value %f\n", lineno, i, cp->cols[i]);
		#endif
		if (cp->cols[i] == 0 && nsp==sp) { // no conversion done. error
			break;
		} else if (errno == ERANGE) {
			break;
		} else {
			sp = nsp;
			if (sp[0] == 0) break;
		}
	}
	if (i != CP_COLCNT) {
		fprintf(stderr, "Invalid data in line %lu\n", lineno);
		exit(1);
	} else {
		capsense_proc(cp);
		update_smoothed_limits(cp);
		detect_pressevents(cp);
	}
}
