#include <stdio.h>

#include <sys/types.h> // stat
#include <sys/stat.h> // stat
#include <unistd.h> // stat

#include <stdlib.h>    // strtol()
#include <errno.h>
#include <limits.h>    // INT_MIN/MAX etc.
#include <string.h>    // memset()

#include <capsense.h>
#include <ringbuffer/ringbuffer.h>
#include "termsize.h"

cp_st cp_real;
cp_st *cp = &cp_real;
int tw, th;

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

int scaletx(float v, float rmin, float rmax) {
	return ((v-rmin) / (rmax-rmin)) * (tw-1);
}

void print_rgb_bg(int r, int g, int b) {
	printf("\033[48;2;%d;%d;%dm", r, g, b);
}

void print_rgb_fg(int r, int g, int b) {
	printf("\033[38;2;%d;%d;%dm", r, g, b);
}

void prow(cp_st *cp, char *row, char **rowfgs) {
	static char *prior_rowfg=0;
	if (cp->bst == BST_OPEN_DEBOUNCE) {
		print_rgb_bg(0,0,90);
	} else if (cp->bst == BST_OPEN) {
		print_rgb_bg(0,0,250);
	} else if (cp->bst == BST_CLOSED_DEBOUNCE) {
		print_rgb_bg(0,90,0);
	} else if (cp->bst == BST_CLOSED) {
		print_rgb_bg(0,255,0);
	} else {
		printf("\033[41m[%d] ", cp->bst);
	}
	for (int i=0; i<255; i++) {
		if (!row[i]) break;
		if (!rowfgs[i]) {
			putc(row[i], stdout);
		} else if (!prior_rowfg) {
			printf("\033[%sm%c", rowfgs[i], row[i]);
		} else if (strcmp(rowfgs[i], prior_rowfg)) {
			printf("\033[%sm%c", rowfgs[i], row[i]);
			prior_rowfg = rowfgs[i];
		} else {
			putc(row[i], stdout);
		}
		/* fputs(row, stdout); */
	}
	fputs("\033[;m\n", stdout);
}

void pat(cp_st *cp, int sx, float v, char ch, char *attr, char send, char clear) {
	// if clear is true, resets buffer and leaves (no printing)
	static char row[255];
	static char *rowfgs[255];
	static int cmax=0;
	if (clear) {
		cmax=0;
		memset(rowfgs, 0, sizeof(char *) * 255);
	} else if (send) {
		prow(cp, row, rowfgs);
	} else {
		/* printf("Sx: %d\n", sx); */
		if (sx > cmax) {
			for (int i=cmax; i<sx-1; i++) row[i]=' ';
			row[sx-1] = ch;
			rowfgs[sx-1] = attr;
			row[sx] = 0;
		} else {
			row[sx-1] = ch;
		}
		cmax = sx;
		/* printf("\033[%dCV\r", x); */
	}
}

void pcols(cp_st *cp) {
	static unsigned long idx=0;
	static char init=0;
	/* static int pcols[CP_COLCNT]; */
	static int ccols[CP_COLCNT];
	float v;
	int sx;
	/* int px; */
	float rmin = cp->rmin;
	float rmax = cp->rmax;

	for (int i=CP_COL_DATASTART; i<CP_COLCNT; i++) {
		v = cp->cols[i];
		sx = scaletx(v, rmin, rmax);
		ccols[i] = sx;
	}
	if (!init) {
		/* for (int i=CP_COL_DATASTART; i<CP_COLCNT; i++) pcols[i] = ccols[i]; */
		init = 1;
	}
	if (!(idx%16)) {
		pat(cp, 0,0,0,0,0,1); // clear
	}
	pat(cp, scaletx(rmin, rmin, rmax)+1, rmin, '{', "33;1", 0, 0);
	for (int i=CP_COL_DATASTART; i<CP_COLCNT; i++) {
		char colchar = colchars[i];
		if (colchar) {
			sx = ccols[i];
			/* px = pcols[i]; */
			pat(cp, sx, v, colchar, colfgs[i], 0, 0);
			/* if (sx >= px) { */
				/* for (int ix=px; ix<=sx; ix++) pat(ix, v, colchar, 0); */
			/* } else { */
				/* for (int ix=sx; ix<=px; sx++) pat(ix, v, colchar, 0); */
			/* } */
		}
	}
	pat(cp, scaletx(rmax, rmin, rmax), rmax, '}', "33;1", 0, 0);
	if ((idx%16) == 15) pat(cp, 0,0,0,0,1,0); // send
	/* for (int i=CP_COL_DATASTART; i<CP_COLCNT; i++) pcols[i] = cp->cols[i]; */
	idx++;
	/* printf("\n"); */
}

int main(int argc, char *argv[]) {
	char *logfn;
	char buf[CP_LINEBUFSIZE+1];
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
		pcols(cp);
	}
}

// vim: sw=4 ts=4
