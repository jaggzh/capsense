#include <sys/ioctl.h>
#include <stdio.h>

int get_termsize(int *cols, int *rows) {
	struct winsize w;
	ioctl(0, TIOCGWINSZ, &w);
	if (rows) *rows = w.ws_row;
	if (cols) *cols = w.ws_col;
	/* printf("lines %d\n", w.ws_row); */
	/* printf("columns %d\n", w.ws_col); */
	return 0;
}
