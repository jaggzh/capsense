#define _IN_RINGBUFFER_C
#include "ringbuffer.h"
#include <stdlib.h>
#if RB_DEBUG > 0
#include <stdio.h>
#endif

void ringbuffer_init(rb_st *rb, RB_IDXTYPE len) {
	//struct *rb = malloc(sizeof struct ringbuffer_st);
	rb->d = malloc(sizeof(RB_DTYPE) * len);
	rb->hd=rb->tl=0;
	rb->sz = len;
}

void ringbuffer_setall(rb_st *rb, RB_DTYPE v) {
	for (int i=0; i<rb->sz; i++) rb->d[i] = v;
}

void ringbuffer_add(rb_st *rb, RB_DTYPE v) {
	rb->d[rb->hd] = v;
	rb->hd++;
	if (rb->hd >= rb->sz) rb->hd = 0;
}

#if RB_DEBUG > 0
void ringbuffer_print(rb_st *rb) {
	for (int i=0; i<rb->sz; i++) printf("%f ", rb->d[i]);
	puts("");
}
#endif
