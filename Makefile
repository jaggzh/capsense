all: tests

tests: capsense_test_run

debug: capsense_test_debug

capsense_test_run: tests/capsense-test
	if [ "${CPLOG}" = "" ]; then echo "Set env var CPLOG to log file" >&2; exit 1; fi
	tests/capsense-test "${CPLOG}"

capsense_test_debug: tests/capsense-test
	if [ "${CPLOG}" = "" ]; then echo "Set env var CPLOG to log file" >&2; exit 1; fi
	gdb --args tests/capsense-test "${CPLOG}"


tests/capsense-test: tests/capsense-test.c capsense.c capsense.h \
	../ringbuffer/ringbuffer.c tests/millis.c tests/termsize.c
	gcc -ggdb3 -I. -I.. \
		-Wall -o tests/capsense-test \
		tests/capsense-test.c \
		capsense.c \
		capproc.c \
		ringbuffer.c \
		ringbuffer.h \
		tests/millis.c \
		tests/termsize.c

tags: unfulfilled
	ctags $$(find . -name '*.[ch]' -type f)
unfulfilled:

vi:
	vim Makefile \
		tests/capsense-test.c \
		capsense.c \
		capsense.h \
		capproc.c \
		capproc.h \
		sense-presses.py \
		tests/bansi.c \
		tests/bansi.h \
		ringbuffer.c \
		ringbuffer.h \
		tests/millis.c \
		tests/millis.h \
		tests/termsize.c \
		tests/termsize.h
