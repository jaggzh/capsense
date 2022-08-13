all: tests

tests: capsense_test_run

debug: capsense_test_debug

capsense_test_run: tests/capsense-test
	if [ "${CPLOG}" = "" ]; then echo "Set env var CPLOG to log file" >&2; exit 1; fi
	tests/capsense-test "${CPLOG}"

capsense_test_debug: tests/capsense-test
	if [ "${CPLOG}" = "" ]; then echo "Set env var CPLOG to log file" >&2; exit 1; fi
	gdb --args tests/capsense-test "${CPLOG}"


tests/capsense-test: tests/capsense-test.cpp capsense.cpp capsense.h \
	../ringbuffer/ringbuffer.c tests/millis.c tests/termsize.c
	g++ -ggdb3 -I. -I.. \
		-I${HOME}/Arduino/libraries/PrintHero/src \
		-L${HOME}/Arduino/libraries/PrintHero/src \
		-I${HOME}/Arduino/libraries/MagicSerialDechunk/src \
		-L${HOME}/Arduino/libraries/MagicSerialDechunk/src \
		-Wall -o tests/capsense-test \
		tests/capsense-test.cpp \
		capsense.cpp \
		capproc.cpp \
		ringbuffer.c \
		ringbuffer.h \
		tests/millis.c \
		tests/termsize.c

tags: unfulfilled
	ctags $$(find . -name '*.[ch]' -type f)
unfulfilled:

vi:
	vim Makefile \
		tests/capsense-test.cpp \
		capsense.cpp \
		capsense.h \
		capproc.cpp \
		sense-presses.py \
		capproc.h \
		tests/bansi.c \
		tests/bansi.h \
		ringbuffer.c \
		ringbuffer.h \
		tests/millis.c \
		tests/millis.h \
		tests/termsize.c \
		tests/termsize.h
