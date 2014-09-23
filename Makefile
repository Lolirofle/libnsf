OUT=nsf
CC=gcc
DEPS=
CFLAGS= -fPIC -std=c99 -Wall -fms-extensions `pkg-config --cflags $(DEPS)`
LDFLAGS= -shared -Wl,-soname,lib$(OUT).so `pkg-config --libs $(DEPS)`

vpath %.c .

rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

SOURCES=$(call rwildcard,./src/,*.c)

all: CFLAGS+= -O3
all: $(OUT)

debug: CFLAGS+= -g -ftrapv -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wcast-qual -Wswitch-default -Wunreachable-code -Wfloat-equal -Wuninitialized -Wignored-qualifiers -Wsuggest-attribute=pure -Wsuggest-attribute=const
debug: $(OUT)

clean:
	rm -f lib$(OUT).so

$(OUT): $(SOURCES)
	$(CC) -I. $(CFLAGS) -o lib$@.so $(LDFLAGS) $^
