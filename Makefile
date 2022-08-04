all: tests

tests: capsense_test_run

capsense_test_run: tests/capsense-test
	if [ "${CPLOG}" = "" ]; then echo "Set env var CPLOG to log file" >&2; exit 1; fi
	tests/capsense-test "${CPLOG}"

tests/capsense-test: tests/capsense-test.c tests/millis.c capsense.c capsense.h
	gcc -ggdb3 -I. -I.. \
		-Wall -o tests/capsense-test \
		tests/capsense-test.c capsense.c tests/millis.c \
		../ringbuffer/ringbuffer.c

vi:
	vim Makefile \
		tests/capsense-test.c capsense.c capsense.h \
		../ringbuffer/ringbuffer.c \
		../ringbuffer/ringbuffer.h \
		tests/millis.c \
		tests/millis.h
