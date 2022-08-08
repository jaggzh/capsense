#ifndef _CAPSENSE_PROC_H
#define _CAPSENSE_PROC_H

#include "ringbuffer.h"
#include "capproc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CP_DEBUG 0
/* #define CP_DEBUG 1 */

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
#define CP_COLCNT 9
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

struct capsense_st {
	CP_DTYPE cols[CP_COLCNT];
	struct ringbuffer_st rb_range_real;
	struct ringbuffer_st *rb_range;
	CP_DTYPE mn, mx;
	float smoothmin, smoothmax; // recent min and max (from buffered)
	char init;
	int bst;
};
typedef struct capsense_st cp_st;

void ringbuffer_minmax(cp_st *cp, RB_DTYPE mn, RB_DTYPE mx);
void capsense_init(cp_st *cp);
void capsense_proc(cp_st *cp);
void capsense_procstr(cp_st *cp, char *buf);
void set_cb_press(void (*cb)());
void set_cb_release(void (*cb)());

#ifndef _IN_CAPSENSE_PROC_C
	extern char colchars[CP_COLCNT];
	extern char *colnames[CP_COLCNT];
	extern char *colfgs[CP_COLCNT];
#endif // /_IN_CAPSENSE_PROC_C


#ifdef __cplusplus
}
#endif

#endif // /_CAPSENSE_PROC_H

