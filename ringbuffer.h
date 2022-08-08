#ifndef _RINGBUFFER_H
#define _RINGBUFFER_H
#ifndef _IN_RINGBUFFER_C
#endif /* /_IN_RINGBUFFER_C */

#ifdef __cplusplus
extern "C" {
#endif

#define RB_DEBUG 1

#include <stdint.h>

#define RB_DTYPE   float
#define RB_IDXTYPE uint8_t

struct ringbuffer_st {
    RB_DTYPE *d;          // data
    RB_IDXTYPE hd, tl;    // head tail indices
    RB_IDXTYPE sz;        // *total* buffer element count
};
typedef struct ringbuffer_st rb_st;

void ringbuffer_init(rb_st *rb, RB_IDXTYPE len);
void ringbuffer_setall(rb_st *rb, RB_DTYPE v);
void ringbuffer_add(rb_st *rb, RB_DTYPE v);
#if RB_DEBUG > 0
void ringbuffer_print(rb_st *rb);
#endif

#ifdef __cplusplus
}
#endif

#endif
