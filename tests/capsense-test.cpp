#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include <sys/types.h> // stat
#include <sys/stat.h> // stat
#include <unistd.h> // stat

#include <stdlib.h>    // strtol()
#include <errno.h>
#include <limits.h>    // INT_MIN/MAX etc.
#include <string.h>    // memset()

#include <capsense.h>
#include <capproc.h>
#include <ringbuffer/ringbuffer.h>
#include "termsize.h"

/* #define GRAPH_CLUSTER_LINES 16 */
#define GRAPH_CLUSTER_LINES 1
#define DBBREAK { *((char *)0) = 0; } // segfault gdb
#define RINGBUFFER_MINMAX_LOOKBEHIND_COUNT  20
#define COLORCODE_BUFS 5
#define ITOA_HELPER(x) #x
#define ITOA(x) ITOA_HELPER(x)
#define RGB_FG(r,g,b) "\033[38;2;" ITOA(r) ";" ITOA(g) ";" ITOA(b) "m"
#define RGB_BG(r,g,b) "\033[48;2;" ITOA(r) ";" ITOA(g) ";" ITOA(b) "m"

#define CPT_DEBUG_PAT 2
#undef CPT_DEBUG_PAT

cp_st cp_real;
cp_st *cp = &cp_real;
int tw, th;
rb_st rb_raw_real;
rb_st *rb_raw;
float rb_raw_min=(float)INT_MAX, rb_raw_max=(float)INT_MIN;

/*
def mark_pressrange(st, en, d=None, plot=None):
	plot.axvspan(st, en)
def mark_start_try(plot=None, ms=None, y=None):
	print("start (maybe)")
	plot.plot(ms, y, marker=5, markersize=10,
			markeredgecolor='white',
			markerfacecolor='green',
			alpha=.85)
def mark_start_true(plot=None, ms=None, y=None):
	print("START YESSSSSSS")
	plot.plot(ms, y, marker='d', markersize=8,
			markeredgecolor='darkblue',
			markerfacecolor='green',
			alpha=1.0)
def mark_start_abort(plot=None, ms=None, y=None):
	print("START **ABORT**")
	plot.plot(ms, y, marker='X', markersize=13,
			markeredgecolor='red',
			markerfacecolor='darkred',
			alpha=1.0)
def mark_end_try(plot=None, ms=None, y=None):
	print("end (maybe)")
	plot.plot(ms, y, marker=5, markersize=10,
			markeredgecolor='white',
			markerfacecolor='red',
			alpha=.85)
def mark_end_true(plot=None, ms=None, y=None):
	print("END YESSSSSSS")
	plot.plot(ms, y, marker='o', markersize=8,
			markeredgecolor='black',
			markerfacecolor='red',
			alpha=1.0)
def mark_end_true(plot=None, ms=None, y=None):
	print("END YESSSSSSS")
	plot.plot(ms, y, marker='X', markersize=12,
			markeredgecolor='black',
			markerfacecolor='red',
			alpha=1.0)
def mark_safety_fail(plot=None, ms=None, y=None):
	print("SAFETY FAIL")
	plot.plot(ms, y, marker='h', markersize=13,
			markeredgecolor='red',
			markerfacecolor='purple',
			alpha=1.0)
*/

int isfile(char *fn) {
	// fstat
	struct stat sb;
	if (stat(fn, &sb)) return 0;
	if ((sb.st_mode & S_IFMT) == S_IFREG) return 1;
	errno = ENOENT;
	return 0;
}

int scaletx(float v, float vmin, float vmax) {
	return (((v-vmin) / (vmax-vmin)) * (tw-1)) + 1;
}

const char *rgb_str_fg(int r, int g, int b) {
	static int i=0;
	static char buf[COLORCODE_BUFS][20];
	if (r>255 || g>255 || b>255) return "";
	sprintf(buf[i], "\033[38;2;%d;%d;%dm", r, g, b);
	if (++i >= COLORCODE_BUFS) i=0;
	return buf[i];
}
const char *rgb_str_bg(int r, int g, int b) {
	static int i=0;
	static char buf[COLORCODE_BUFS][20];
	if (r>255 || g>255 || b>255) return "";
	sprintf(buf[i], "\033[48;2;%d;%d;%dm", r, g, b);
	if (++i >= COLORCODE_BUFS) i=0;
	return buf[i];
}

void print_rgb_fg(int r, int g, int b) {
	printf("\033[38;2;%d;%d;%dm", r, g, b);
}
void print_rgb_bg(int r, int g, int b) {
	printf("\033[48;2;%d;%d;%dm", r, g, b);
}

void prow(cp_st *cp, const char *row, const char **rowfgs) {
	static const char *prior_rowfg=0;
	if (cp->bst == BST_OPEN_DEBOUNCE) {
		print_rgb_bg(0,0,50);
	} else if (cp->bst == BST_OPEN) {
		print_rgb_bg(0,0,150);
	} else if (cp->bst == BST_CLOSED_DEBOUNCE) {
		print_rgb_bg(0,40,0);
	} else if (cp->bst == BST_CLOSED) {
		print_rgb_bg(0,155,0);
	} else {
		printf("%s[%d] ", rgb_str_bg(127,0,127), cp->bst);
	}
	for (int i=0; i<255; i++) {
		if (!row[i]) break;
		#ifdef CPT_DEBUG_PAT
			if (row[i] != 32) printf("Row[%d] %c\n", i, row[i]);
		#endif
		if (!rowfgs[i]) {
			putc(row[i], stdout);
		} else if (!prior_rowfg) {
			printf("%s%c", rowfgs[i], row[i]);
		} else if (strcmp(rowfgs[i], prior_rowfg)) {
			printf("%s%c", rowfgs[i], row[i]);
			prior_rowfg = rowfgs[i];
		} else {
			putc(row[i], stdout);
		}
		/* fputs(row, stdout); */
	}
	fputs("\033[;m\n", stdout);
}

void pat(cp_st *cp, int sx, float v, char ch, const char *attr, char send, char clear) {
	// if clear is true, resets buffer and leaves (no printing)
	static char row[255];
	static const char *rowfgs[255];
	static int zeroi=0;
	sx-=1;  // switch to 0 offset
	if (clear) {
		zeroi=0;
		memset(rowfgs, 0, sizeof(char *) * 255);
		memset(row, 0, sizeof(row));
	} else if (send) {
		prow(cp, row, rowfgs);
	} else {
		#ifdef CPT_DEBUG_PAT
			printf("PAT %c at sx=%d\n", ch, sx);
		#endif
		if (sx<0 || sx>=tw) return;
		if (sx >= zeroi) {
			for (int i=zeroi; i<sx; i++) row[i]=' ';
			row[sx] = ch;
			rowfgs[sx] = attr;
			row[sx+1] = 0;
			zeroi = sx+1;
		} else {
			row[sx] = ch;
			rowfgs[sx] = attr;
		}
		#ifdef CPT_DEBUG_PAT
			printf("Cmax: %d\n", zeroi);
		#endif
		/* printf("\033[%dCV\r", x); */
	}
}

void cptest_ringbuffer_set_minmax(float *minp, float *maxp, rb_st *rb) {
	float lmn=(float)INT_MAX;
	float lmx=(float)INT_MIN;
	RB_DTYPE v;
	for (int i=0; i<rb->sz; i++) {
		v = rb->d[i];
		if (lmn > v) lmn = v;
		if (lmx < v) lmx = v;
	}
	if (*minp == (float)INT_MAX) *minp = lmn; 
	else {
		if (lmn < *minp) *minp = lmn;  // force limit so graph will always be
		else *minp += (lmn - *minp)/6; // within range. otherwise smooth the change
	}

	if (*maxp == (float)INT_MIN) *maxp = lmx; 
	else {
		if (lmx > *maxp) *maxp = lmx;
		else *maxp += (lmx - *maxp)/6;
	}
}

void pcols(cp_st *cp) {
	static unsigned long idx=0;
	static char init=0;
	/* static int pcols[CP_COLCNT]; */
	static int sxvals[CP_COLCNT];
	float v;
	int sx;
	/* int px; */
	/* float rmin = cp->rmin; */
	/* float rmax = cp->rmax; */

	if (!init) {
		/* for (int i=CP_COL_DATASTART; i<CP_COLCNT; i++) pcols[i] = sxvals[i]; */
		init = 1;
		ringbuffer_setall(rb_raw, cp->cols[CP_COL_DATASTART]);
	}
	//for (int i=CP_COL_DATASTART; i<CP_COLCNT; i++)
	for (int i=CP_COL_DATASTART; i<CP_COLCNT; i++)
		ringbuffer_add(rb_raw, cp->cols[i]);
	ringbuffer_add(rb_raw, cp->mn);
	ringbuffer_add(rb_raw, cp->mx);
	/* ringbuffer_print(rb_raw); */
	cptest_ringbuffer_set_minmax(&rb_raw_min, &rb_raw_max, rb_raw);
	#ifdef CPT_DEBUG_PAT
		printf("raw_min = %f, raw_max = %f\n", rb_raw_min, rb_raw_max);
		printf(" cp->mn = %f,  cp->mx = %f\n", cp->mn, cp->mx);
	#endif

	for (int i=CP_COL_DATASTART; i<CP_COLCNT; i++) {
		v = cp->cols[i];
		sx = scaletx(v, rb_raw_min, rb_raw_max);
		sxvals[i] = sx;
		#ifdef CPT_DEBUG_PAT
			printf("[%d] Min: %f, Max: %f, Val: %f, SX: %d, Ch:%c\n",
				i, rb_raw_min, rb_raw_max, v, sx, !colchars[i] ? '_' : colchars[i]);
		#endif
	}
	if (GRAPH_CLUSTER_LINES == 1 || !(idx%GRAPH_CLUSTER_LINES)) {
		pat(cp, 0,0,0,0,0,1); // clear
	}
	//for (int i=CP_COL_DATASTART; i<CP_COLCNT; i++) {
	//
	pat(cp, scaletx(cp->smoothmin, rb_raw_min, rb_raw_max)-1, cp->mn, '{', RGB_FG(250,0,0), 0, 0);
	pat(cp, scaletx(cp->smoothmax, rb_raw_min, rb_raw_max)+1, cp->mx, '}', RGB_FG(0,250,0), 0, 0);
	for (int i=0; i<CP_COLCNT; i++) {
		char colchar = colchars[i];
		if (colchar) {
			sx = sxvals[i];
			/* px = pcols[i]; */
			pat(cp, sx, v, colchar, colfgs[i], 0, 0);
			/* if (sx >= px) { */
				/* for (int ix=px; ix<=sx; ix++) pat(ix, v, colchar, 0); */
			/* } else { */
				/* for (int ix=sx; ix<=px; sx++) pat(ix, v, colchar, 0); */
			/* } */
		}
	}
	/* if ((idx%16) == 15) pat(cp, 0,0,0,0,1,0); // send */
	if (GRAPH_CLUSTER_LINES == 1 ||
		((idx%GRAPH_CLUSTER_LINES) == GRAPH_CLUSTER_LINES-1)) {
			pat(cp, 0,0,0,0,1,0); // send
	}

	/* for (int i=CP_COL_DATASTART; i<CP_COLCNT; i++) pcols[i] = cp->cols[i]; */
	idx++;
	/* printf("\n"); */
}

int main(int argc, char *argv[]) {
	char *logfn;
	char buf[CP_LINEBUFSIZE+1];
	rb_raw = &rb_raw_real;
	ringbuffer_init(rb_raw, RINGBUFFER_MINMAX_LOOKBEHIND_COUNT * CP_COLCNT);
	setbuf(stdout, 0);
	get_termsize(&tw, &th);
	if (argc>0) {
		logfn=argv[1];
		if (!isfile(logfn)) {
			fprintf(stderr, "Not a file? %s\n", logfn);
			exit(1);
		}
	}
	FILE *f = fopen(logfn, "r");
	if (!f) {
		fprintf(stderr, "Error opening file: %s\n", logfn);
		exit(errno);
	}
	capsense_init(cp);
	while (fgets(buf, CP_LINEBUFSIZE, f)) {
		capsense_procstr(cp, buf);
		/* printf("\033[41;1mSTATE: bst=%d\033[0m\n", cp->bst); */
		pcols(cp);
	}
}

#ifdef __cplusplus
}
#endif

// vim: sw=4 ts=4