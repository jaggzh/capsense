#include <stdio.h>

#include <sys/types.h> // stat
#include <sys/stat.h> // stat
#include <unistd.h> // stat

#include <stdlib.h>    // strtol
#include <errno.h>

#include <capsense.h>
#include <ringbuffer/ringbuffer.h>

cp_st cp_real;
cp_st *cp = &cp_real;

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

#if 0
void plot_presses(d=None, mins=None, maxs=None, avgs=None,
                 plot=None, debounce_samples=5):
    global bst
    global press_start_i
    global press_start_ms
    global safety_time_start_ms
    st=st2=en=en2=None
    for i in range(0, len(d)-1):
        /* # Noiserange: Based on recent max/mins of signal (vdiv4) */
        /* #             This lets us pick a multiplier for a threshold */
        /* #             to know what to consider a press or release */
        /* #             (ie. 2x above slow_average (dst128) for press, */
        /* #             and 1.5x for release) */
        noiserange = maxs[i] - mins[i]
        starty = mins[i]
        /* #startyref = d['vdiv128'][i] */
        startyref = d['vdiv128'][i]
        startdelta = starty - startyref
        endy = mins[i]
        /* #endyref = d['vdiv128'][i] */
        endyref = d['vdiv256'][i]
        enddelta = endyref - endy
        chaosy = mins[i]
        chaosyref = d['vdiv32'][i]
        chaosdelta = chaosy - chaosyref
        ms = d['millis'][i]
        close_thresh = .8
        open_thresh = .3

        if safety_time_start_ms is not None:
            dist = ms-safety_time_start_ms
            print("Safety:", safety_time_start_ms)
            print("Millis:", ms)
            print("Millis-Safety:", dist)
            if dist > chaos_safety_time_ms:
                /* # mark_safety_end(plot=plot, d=d, ms=safety_time_start_ms) */
                safety_time_start_ms = None
            else:
                continue

        /* # If safety timeout was set and it's too soon, quit */
        /* # if safety_time_start_ms is not None: */
        /* #     if d['millis'][i] - safety_time_start_ms < 2000: return */

        if bst == 'open':  # Default state: Open == not a cap-sense press
            if startdelta > noiserange*close_thresh:
                st = i
                stref = startyref
                bst = 'closed_debounce'
                mark_start_try(plot=plot, ms=d['millis'][i], y=starty)
        elif bst == 'closed_debounce':
            if i-st > debounce_samples:
                if startdelta > noiserange*close_thresh:
                    bst = 'closed'
                    press_start_i = i
                    press_start_ms = ms
                    mark_start_true(plot=plot, ms=d['millis'][i], y=starty)
                else:
                    bst = 'open'
                    mark_start_abort(plot=plot, ms=d['millis'][i], y=starty)
        elif bst == 'closed':
            if fail_safety_points(i=i, ms=d['millis'][i], y=starty,
                    noiserange=noiserange,
                    chaosdelta=chaosdelta, chaosy=chaosy, chaosyref=chaosyref,
                    curd=d['vdiv16'][press_start_i]):
                /* # Tests and issues mark_safety_fail() if needed to plot */
                bst = 'open'
            elif enddelta > noiserange*open_thresh:
                en = i
                mark_end_try(plot=plot, ms=d['millis'][i], y=endy)
                bst = 'open_debounce'
        elif bst == 'open_debounce':
            if i-en > debounce_samples:
                if enddelta > noiserange*open_thresh:
                    bst = 'open'
                    mark_end_true(plot=plot, ms=d['millis'][i], y=endy)
                    mark_pressrange(d['millis'][st], d['millis'][i], d=d, plot=plot)
                else:
                    bst = 'closed'
                    mark_end_false(plot=plot, ms=d['millis'][i], y=endy)
                    plot.vlines(x=[ms], ymin=endyref, ymax=endy, color='red', lw=4)
#endif

int isfile(char *fn) {
    // fstat
    struct stat sb;
    if (stat(fn, &sb)) return 0;
    if ((sb.st_mode & S_IFMT) == S_IFREG) return 1;
    errno = ENOENT;
    return 0;
}

void cp_plot(cp_st *cp) {
    printf("%f\n", cp->cols[COL_RAW_I]);
}

int main(int argc, char *argv[]) {
    char *logfn;
    char buf[CP_LINEBUFSIZE+1];
    setbuf(stdout, 0);
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
        cp_plot(cp);
    }
}

// vim: sw=4 ts=4 et


