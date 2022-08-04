#define _IN_CAPSENSE_PROC_C
#include <ringbuffer/ringbuffer.h>
#include <limits.h>  // INT_MIN/MAX
#include <stdlib.h>  // strtof()
#include <errno.h>
#include <stdio.h>  // printf

#include "capsense.h"

#ifndef ARDUINO
#include "tests/millis.h"
#endif

/* Handle capacitive sensor samples */
// 164 samples/sec currently
int sps = 164;
int debounce_samples = 5;   // ~ 30ms
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
int grip_eval_delay_samples;     // Set in capsense_init()
int chaos_safety_time_ms = 1000; // datetime.timedelta(milliseconds=1000);
int safety_time_start_ms = -1; // None

int bst = BST_OPEN;           //  button state;
int cmillis = 0;
int press_start_i = -1;
int press_start_ms = -1;

char *colnames[CP_COLCNT] = {
	"millis", "raw", "vdiv4", "vdiv8", "vdiv16",
	"vdiv32", "vdiv64", "vdiv128", "vdiv128a"
};
int lookbehind=8;

void capsense_init(cp_st *cp) {
    cp->rb = &cp->rb_real;
    ringbuffer_init(cp->rb, lookbehind);
    ringbuffer_setall(cp->rb, 0);
	grip_eval_delay_samples = (int)(sps * grip_eval_checkpoint_ms * .001);
	if (!cmillis) cmillis = millis(); // Only do this once even with multiple caps!
}

void cp_ringbuffer_set_minmax(cp_st *cp) {
	rb_st *rb = cp->rb;
	float lmn=(float)INT_MAX;
	float lmx=(float)INT_MIN;
	CP_DTYPE v;
	for (int i=0; i<rb->sz; i++) {
		v = rb->d[i];
		if (lmn < v) lmn = v;
		if (lmx > v) lmx = v;
	}
	cp->mn = lmn;
	cp->mx = lmx;
    // avgs.append(sum(d['raw'][st:en]) / (en-st))
}

void capsense_proc(cp_st *cp) {
    /* # Make our own dividing moving average */
    /* def add_moving_div(d=None, div=None, label=None): */
    /*     a = [d['raw'][0]] # first val */
    /*     for i in range(1, len(d)): */
    /*         newv = a[i-1] + (d['raw'][i] - a[i-1])/div */
    /*         a.append(newv) */
    /*     d[label]=a */
    /* add_moving_div(d=d, div=256, label='vdiv256') */

	if (!cp->init) {      // first call we init on data, to suppress jumping vals
		ringbuffer_setall(cp->rb, cp->cols[COL_VDIV16_I]);
		cp->init=1;
	}
	ringbuffer_add(cp->rb, cp->cols[COL_VDIV16_I]); // Used for moving min/max
	cp_ringbuffer_set_minmax(cp);
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
	}
}

void set_safety_timeout(int ms, int y, int yref) {
    safety_time_start_ms = ms;
    /* plot.axvline(ms, color='magenta') */
    /* if y is not None and yref is not None: */
    /*     plot.vlines(x=[ms], ymin=yref, ymax=y, color='blue', lw=4) */
}

void mark_safety_end(int ms) {
    /* global safety_time_start_ms */
    /* plot.axvline(ms, color='brown') */
    safety_time_start_ms = -1;
}

void fail_safety_points(int i, int ms, int y,
        float noiserange, float chaosdelta,
        int chaosy, int chaosyref, int curd) {
    if (i - press_start_i > grip_eval_delay_samples) {
        if (y > curd + noiserange*2) {
            safety_time_start_ms = ms;
            /* mark_safety_fail(plot=plot, ms=d['millis'][i], y=y) */
            bst = BST_OPEN;
        }
    }
    // if chaosdelta < noiserange*.8:  # Don't let crazy time trigger
    //     bst = 'open'
    //     set_safety_timeout(plot=plot, d=d, ms=d['millis'][i], y=chaosy, yref=chaosyref)
}

