#!/usr/bin/env python
import matplotlib.pyplot as plt
from matplotlib import dates
from matplotlib.ticker import (MultipleLocator,
                               FormatStrFormatter,
                               AutoMinorLocator,
                               LinearLocator)
import pandas as pd
import numpy as np
import ipdb
import time
import datetime
import sys
import os
from bh.util import *
from bh.bansi import *

# 164 samples/sec currently
sps = 164
debounce_samples = 5   # ~ 30ms
debounce_wait_time = debounce_samples * (1/164)
# .0060975609s / sample

################################
# Gripping/movement settings
# grip_eval_delay_ms
# When gripping from a distance, capacitance will rise a lot more than
# a normal grip.
# This seems to take from 1 to 2s for a somewhat relaxed pace of hand
# placement.
grip_eval_checkpoint_ms = 250
grip_eval_samples_retest = int(sps * grip_eval_checkpoint_ms * .001)
grip_eval_samples_cancel = int(sps * grip_eval_checkpoint_ms * .001 * 1.5)
grip_eval_rise_scale = 10
print("grip_eval_samples_retest:", grip_eval_samples_retest)
# sys.exit()
chaos_safety_time = datetime.timedelta(milliseconds=3000)
safety_time_start = None
bst='open'  # button state
press_start_i = None
press_start_ms = None
press_start_y = None
max_since_press = -10000000000

def mark_pressrange(st, en, d=None, plot=None):
    plot.axvspan(st, en, color='darkgreen')

def mark_start_try(plot=None, ms=None, y=None):
    print("start (maybe)")
    plot.plot(ms, y, marker=5, markersize=10, # caretright / triangle
            markeredgecolor='white',
            markerfacecolor='green',
            alpha=.85)
def mark_start_true(plot=None, ms=None, y=None):
    print("START YESSSSSSS")
    plot.plot(ms, y, marker='d', markersize=8, # diamond
            markeredgecolor='darkblue',
            markerfacecolor='green',
            alpha=1.0)
def mark_start_abort(plot=None, ms=None, y=None):
    print("START **ABORT**")
    plot.plot(ms, y, marker='X', markersize=13, # Big fat X
            markeredgecolor='red',
            markerfacecolor='darkred',
            alpha=1.0)
def mark_end_try(plot=None, ms=None, y=None):
    print("end (maybe)")
    plot.plot(ms, y, marker=5, markersize=10,   # caretright / triangle
            markeredgecolor='white',
            markerfacecolor='red',
            alpha=.85)
def mark_end_true(plot=None, ms=None, y=None):
    print("END YESSSSSSS")
    plot.plot(ms, y, marker='o', markersize=8,  # big circle
            markeredgecolor='black',
            markerfacecolor='red',
            alpha=1.0)
def mark_end_false(plot=None, ms=None, y=None):
    print("END False")
    plot.plot(ms, y, marker='x', markersize=12, # big THIN x
            markeredgecolor='black',
            markerfacecolor='red',
            alpha=1.0)
def mark_safety_fail(plot=None, ms=None, y=None):
    print("SAFETY FAIL")
    plot.plot(ms, y, marker='h', markersize=13,  # hexagon (stop sign)
            markeredgecolor='red',
            markerfacecolor='purple',
            alpha=1.0)

def set_safety_timeout(plot=None, d=None, ms=None, y=None, yref=None):
    global safety_time_start
    safety_time_start = ms
    plot.axvline(ms, color='magenta')
    if y is not None and yref is not None:
        plot.vlines(x=[ms], ymin=yref, ymax=y, color='blue', lw=4)

def mark_safety_end(plot=None, d=None, ms=None):
    global safety_time_start
    plot.axvline(ms, color='brown')
    safety_time_start = None

# Called at time of CLOSED_DEBOUNCE -> CLOSED
# to try to make sure we're not in some extra movements or hand-placement time
def fail_safety_points(plot=None, d=None, i=None, ms=None, y=None,
                       noiserange=None,
                       chaosdelta=None, chaosy=None, chaosyref=None):
    global bst
    global safety_time_start
    global press_start_i
    global press_start_ms

    if i - press_start_i > grip_eval_samples_retest/2 and \
        i - press_start_i < grip_eval_samples_retest and \
            y > d['vdiv16'][press_start_i] + noiserange*grip_eval_rise_scale:
                press_start_i = i
                press_start_ms = ms
    if i - press_start_i > grip_eval_samples_retest:
        if y > d['vdiv16'][press_start_i] + noiserange*grip_eval_rise_scale:
            safety_time_start = ms
            mark_safety_fail(plot=plot, ms=d['millis'][i], y=y)
            bst = 'open'
            return True
    # if chaosdelta < noiserange*.8:  # Don't let crazy time trigger
    #     bst = 'open'
    #     set_safety_timeout(plot=plot, d=d, ms=d['millis'][i], y=chaosy, yref=chaosyref)
    return False

def reading_is_open(cval=None,
                press_start_y=None,
                max_since_press=None,
                open_drop_fraction=None):
    height = max_since_press - press_start_y
    if cval < max_since_press - height*open_drop_fraction:
        return True
    return False


dobreak = False
def plot_presses(d=None, mins=None, maxs=None, avgs=None,
                 plot=None, debounce_samples=5):
    global bst
    global press_start_i
    global press_start_ms
    global safety_time_start
    st=st2=en=en2=None

    global max_since_press
    global dobreak

    for i in range(1, len(d)-1):
        # Noiserange: Based on recent max/mins of signal (vdiv4)
        #             This lets us pick a multiplier for a threshold
        #             to know what to consider a press or release
        #             (ie. 2x above slow_average (dst128) for press,
        #             and 1.5x for release)
        d['vdiv256'][i] = d['vdiv256'][i-1] + (d['vdiv64'][i]-d['vdiv256'][i-1])/256
        noiserange = maxs[i] - mins[i]
        starty = mins[i]
        #startyref = d['vdiv128'][i]
        startyref = d['vdiv128'][i]
        startdelta = starty - startyref
        endy = mins[i]
        #endyref = d['vdiv128'][i]
        endyref = d['vdiv256'][i]
        enddelta = endyref - endy
        chaosy = mins[i]
        chaosyref = d['vdiv32'][i]
        chaosdelta = chaosy - chaosyref
        ms = d['millis'][i]
        resetref = d['vdiv128'][i]
        open_tracking_value = mins[i]
        open_drop_fraction = .7
        close_thresh = 1.9
        open_thresh = .3

        if safety_time_start is not None:
            dist = ms-safety_time_start
            print("Safety:", safety_time_start)
            print("Millis:", ms)
            print("Millis-Safety:", dist)
            if dist > chaos_safety_time:
                # mark_safety_end(plot=plot, d=d, ms=safety_time_start)
                safety_time_start = None
            else:
                continue

        # If safety timeout was set and it's too soon, quit
        # if safety_time_start is not None:
        #     if d['millis'][i] - safety_time_start < 2000: return
        pfpl(bbla, "Cols:")
        pfpl(d['millis'][i]);
        for k in d.keys()[1:]: print(f" {d[k][i]:.2f}", end='')
        pf(f"{whi} [Min {mins[i]:.2f}, Max {maxs[i]:.2f}]{rst}")

        if bst == 'open':  # Default state: Open == not a cap-sense press
            pfp(bcya, "BST: OPEN", rst);
            print(f" Startdelta={startdelta:.5f} > (noiserange={noiserange:.5f} * closethresh={close_thresh} = {noiserange*close_thresh:.5f})")
            # import ipdb; ipdb.set_trace()

            if startdelta > noiserange*close_thresh:
                st = i
                stref = startyref
                bst = 'closed_debounce'
                mark_start_try(plot=plot, ms=d['millis'][i], y=starty)
                print("  -> CLOSED DEBOUNCE")
        elif bst == 'closed_debounce':
            pfp(bcya, "BST: CLOSED_DEBOUNCE", rst);
            print(f" (i={i}-st={st})={i-st} > debounce_samples={debounce_samples}")
            if i-st > debounce_samples:
                print(f"  Startdelta={startdelta:.5f} > (noiserange={noiserange:.5f} * closethresh={close_thresh} = {noiserange*close_thresh:.5f})")
                if startdelta > noiserange*close_thresh:
                    print("  -> CLOSED");
                    bst = 'closed'
                    press_start_i = i
                    press_start_ms = ms
                    press_start_y = open_tracking_value
                    max_since_press = open_tracking_value
                    print(f"{yel}Beginning press at value {max_since_press}{rst}")
                    mark_start_true(plot=plot, ms=d['millis'][i], y=starty)
                    d['vdiv256'][i+0] = resetref
                else:
                    print("  -> OPEN");
                    bst = 'open'
                    mark_start_abort(plot=plot, ms=d['millis'][i], y=starty)
        elif bst == 'closed':
            pfp(bcya, "BST: CLOSED", rst);
            #        __ _t
            #   /   /  \    \ h*tolerance
            #  h   /    \__ / ___
            #   \ /      \    \  considered open 
            if max_since_press < open_tracking_value:
                print(f"{bgre}Raising peak from {max_since_press:.5f} to {open_tracking_value:.5f}{rst}")
                max_since_press = open_tracking_value

            print(f"open_tracking_value={open_tracking_value:.5f}, max_since_press={max_since_press:.5f}")
            # if dobreak: ipdb.set_trace()
            # 2022-08-10 23:01:39.683053 
            # if d['millis'][i] > pd.Timestamp(2000, 1, 1, 0, 0, 3, 986000):
            #     print(f"Cur millis: {d['millis'][i]}")
            #     import ipdb; ipdb.set_trace()
            isopen = reading_is_open(cval=open_tracking_value,
                            press_start_y=press_start_y,
                            max_since_press = max_since_press,
                            open_drop_fraction = open_drop_fraction)
            if fail_safety_points(plot=plot, d=d, i=i, ms=d['millis'][i], y=starty,
                    noiserange=noiserange,
                    chaosdelta=chaosdelta, chaosy=chaosy, chaosyref=chaosyref):
                # Tests and issues mark_safety_fail() if needed to plot
                print(" -> OPEN (from fail_safety_points())");
                bst = 'open'
            #elif enddelta > noiserange*open_thresh:
            elif isopen:
                print(" -> OPEN_DEBOUNCE");
                bst = 'open_debounce'
                en = i
                mark_end_try(plot=plot, ms=d['millis'][i], y=endy)
                # Debug:
                # import ipdb; ipdb.set_trace()
                # reading_is_open(cval=open_tracking_value,
                #             press_start_y=press_start_y,
                #             max_since_press = max_since_press,
                #             open_drop_fraction = open_drop_fraction)
            # elif max_since_press - open_tracking_val >
            #         (max_since_press - open_tracking_val)
            #     en = i
            #     mark_end_try(plot=plot, ms=d['millis'][i], y=endy)
            #     print(" -> OPEN_DEBOUNCE");
            #     bst = 'open_debounce'
        elif bst == 'open_debounce':
            pfp(bcya, "BST: OPEN_DEBOUNCE", rst);
            if i-en > debounce_samples:
                isopen = reading_is_open(cval=open_tracking_value,
                                press_start_y=press_start_y,
                                max_since_press = max_since_press,
                                open_drop_fraction = open_drop_fraction)
                if isopen:
                    print("  -> OPEN");
                    bst = 'open'
                    mark_end_true(plot=plot, ms=d['millis'][i], y=endy)
                    mark_pressrange(d['millis'][st], d['millis'][i], d=d, plot=plot)
                else:
                    print("  -> CLOSED");
                    bst = 'closed'
                    mark_end_false(plot=plot, ms=d['millis'][i], y=endy)
                    plot.vlines(x=[ms], ymin=endyref, ymax=endy, color='red', lw=4)

                # if enddelta > noiserange*open_thresh:
                #     print("  -> OPEN");
                #     bst = 'open'
                #     mark_end_true(plot=plot, ms=d['millis'][i], y=endy)
                #     mark_pressrange(d['millis'][st], d['millis'][i], d=d, plot=plot)
                # else:
                #     print("  -> CLOSED");
                #     bst = 'closed'
                #     mark_end_false(plot=plot, ms=d['millis'][i], y=endy)
                #     plot.vlines(x=[ms], ymin=endyref, ymax=endy, color='red', lw=4)

def main():
    lookbehind=8
    if len(sys.argv) > 1:
        fn=sys.argv[1]
        if not os.path.isfile(fn):
            raise(ValueError(f"Not a file: {fn}"))
        logfn = fn
    else:
        logfn = "sample_data/log-holds-n-presses.txt"
    # 71677 492 374.51 364.69 363.71 363.31 363.52
    # 71677 316 359.89 361.65 362.22 362.57 363.15
    # 71690 310 347.41 358.42 360.59 361.75 362.74
    colnames=('millis', 'raw', 'vdiv4', 'vdiv8', 'vdiv16', 'vdiv32', 'vdiv64', 'vdiv128', 'vdiv128a')
    colnames=('millis', 'raw', 'vdiv8', 'vdiv16', 'vdiv32', 'vdiv64', 'vdiv128', 'vdiv128a')
    d = pd.read_csv(logfn, delim_whitespace=True,
            header=None,
            names=colnames,
            on_bad_lines = 'skip',
            )
    test_small_range = True
    test_small_range = False
    if test_small_range:
        shift=0
        #d=d[400+shift:2550+shift].copy()
        #d=d[0:5900+850].copy()
        d=d[5000:].copy()
        d.reset_index(inplace=True, drop=True)
    fig = plt.figure()
    sp1 = fig.add_subplot(111)
    sp1.axes.xaxis.set_major_formatter(dates.DateFormatter("%M:%S.%f"))
    #now = datetime.datetime.now()
    now = datetime.datetime(2000, 1, 1, 0, 0, 0, 0)
    x = [int(i) for i in (d['millis'] - d['millis'][0])]
    x = [now + datetime.timedelta(milliseconds=i) for i in x]
    d['millis'] = x
# for name in colnames[1:-1]:
#     sp1.plot(d[name].array, label=name)

    # Make our own dividing moving average
    def add_moving_div(d=None, div=None, label=None):
        a = [d['raw'][0]] # first val
        for i in range(1, len(d)):
            newv = a[i-1] + (d['raw'][i] - a[i-1])/div
            a.append(newv)
        d[label]=a

    add_moving_div(d=d, div=256, label='vdiv256')

    plt.xlabel("Seconds")
    print(f"Len (x):", len(x))
    # for l in ('raw', 'vdiv4', 'vdiv8', 'vdiv16', 'vdiv32', 'vdiv64', 'vdiv128', 'vdiv128a'):
    for l in ('raw', 'vdiv8', 'vdiv16', 'vdiv32', 'vdiv64', 'vdiv128', 'vdiv128a'):
        print(f"Len ({l}):", len(d[l]))

    sp1.plot(x, d['raw'].array, label='raw', lw=.2)
    # sp1.plot(x, d['vdiv4'].array, label='vdiv4', lw=.4, ls='dotted')
    sp1.plot(x, d['vdiv8'].array, label='vdiv8', lw=.5, ls='dashed')
    sp1.plot(x, d['vdiv16'].array, label='vdiv16', lw=3, ls='dotted')
    sp1.plot(x, d['vdiv32'].array, label='vdiv32', lw=.5)
    sp1.plot(x, d['vdiv64'].array, label='vdiv64', lw=2, ls='dotted')
    sp1.plot(x, d['vdiv128'].array, label='vdiv128', lw=.5)
    #sp1.plot(x, d['vdiv128a'].array, label='vdiv128assym', lw=1)
    sp1.plot(x, d['vdiv256'].array, label='vdiv256', lw=2)
    # plt.xticks(
    #         ticks=(d['millis'] - d['millis'][0]).array,
    #         labels=(d['millis']/1000).array)

    avgs=[]
    mn = mx = d['vdiv16'][0]
    mins = [mn]           # Init first value
    maxs = [mn]           # Init first value
    for i in range(0, len(d)-1):
        st = i-lookbehind
        if st<0: st=0
        en = i+1

        # if i >= 2876: ipdb.set_trace()
        newmn = min(d['vdiv16'][st:en])
        newmx = max(d['vdiv16'][st:en])
        mn = mn + (newmn - mn)/6
        mx = mx + (newmx - mx)/6
        mins.append(mn)
        maxs.append(mx)
        avgs.append(sum(d['raw'][st:en]) / (en-st))
    sp1.plot(x, mins, label='min', lw=3)
    sp1.plot(x, maxs, label='max', lw=2)
    print(f"Len (mins):", len(mins))
    print(f"Len (maxs):", len(maxs))
    ## sp1.plot(avgs, label='avg', lw=2)
    plot_presses(d=d, mins=mins, maxs=maxs, avgs=avgs, plot=sp1)
    sp1.plot(x, d['vdiv256'].array, label='vdiv256 new', lw=4)
    fig.tight_layout()
    plt.legend()
    plt.grid(True)
    sp1.xaxis.set_major_locator(LinearLocator(numticks=15))
    sp1.xaxis.set_minor_locator(AutoMinorLocator(5))

    sp1.tick_params(which='both', width=2)
    sp1.tick_params(which='major', length=7)
    sp1.tick_params(which='minor', length=4, color='r')
    # plt.grid(True, which='minor')
    #sp1.xaxis.set_major_locator(MultipleLocator(.20))
    #sp1.xaxis.set_major_formatter(FormatStrFormatter('%d'))
    #sp1.xaxis.set_minor_locator(MultipleLocator(.5))

    # plt.gca().set_aspect(.5, adjustable='datalim')
    # plt.gca().set_aspect(4)
    # plt.ion()
    # plt.savefig('filename.png', dpi=600)
    #sp1.tick_params(labelrotation=55)
    plt.gcf().autofmt_xdate()
    plt.show()
    # plt.pause(.01)
    # ipdb.set_trace()

main()

# vim: sw=4 ts=4 et
