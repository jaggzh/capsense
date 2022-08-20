all: tests

tests: capsense_test_run

debug: capsense_test_debug

capsense_test_run: tests/capsense-test
	if [ "${CPLOG}" = "" ]; then echo "Set env var CPLOG to log file" >&2; exit 1; fi
	tests/capsense-test "${CPLOG}"

capsense_test_debug: tests/capsense-test
	if [ "${CPLOG}" = "" ]; then echo "Set env var CPLOG to log file" >&2; exit 1; fi
	gdb --args tests/capsense-test "${CPLOG}"


tests/capsense-test: tests/capsense-test.cpp \
		capsense.cpp capsense.h \
		ringbuffer.c \
		tests/millis.c \
		tests/bansi.c \
		tests/termsize.c
	g++ -ggdb3 \
		-Werror \
		-Wno-unused-variable \
		-fPIC -I. -I.. \
		-DCAPTEST_MODE \
		-I${HOME}/Arduino/libraries/PrintHero/src \
		-L${HOME}/Arduino/libraries/PrintHero/src \
		-I${HOME}/Arduino/libraries/MagicSerialDechunk/src \
		-L${HOME}/Arduino/libraries/MagicSerialDechunk/src \
		-Wall \
		-o tests/capsense-test \
		capsense.cpp \
		ringbuffer.c \
		ringbuffer.h \
		tests/bansi.c \
		tests/millis.c \
		tests/termsize.c \
		${HOME}/Arduino/libraries/MagicSerialDechunk/src/MagicSerialDechunk.cpp \
		tests/capsense-test.cpp

tags: ta
ta:
	#ctags $$(find . -name '*.[ch]' -type f)
	find . -regextype egrep -regex '.*\.(c|cc|h|hh|cpp)$$' | ctags -L -

vi:
	vim Makefile \
		tests/capsense-test.cpp \
		capsense.cpp \
		capsense.h \
		tests/bansi.c \
		tests/bansi.h \
		ringbuffer.c \
		ringbuffer.h \
		tests/millis.c \
		tests/millis.h \
		tests/termsize.c \
		tests/termsize.h
