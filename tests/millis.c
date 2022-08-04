#ifndef ARDUINO
#include <sys/time.h>
//#include <unistd.h>

struct timeval __millis_start;

void init_millis(void) {
    gettimeofday(&__millis_start, 0);
};

unsigned long int millis(void) {
    long mtime, seconds, useconds; 
    struct timeval end;
    gettimeofday(&end, 0);
    seconds  = end.tv_sec  - __millis_start.tv_sec;
    useconds = end.tv_usec - __millis_start.tv_usec;

    mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
    return mtime;
};
#endif
