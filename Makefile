CC=gcc
CFLAGS= -std=gnu99 -Wall -fms-extensions
LDFLAGS= -lpopt
OUT=nsfinfo

vpath %.c .

rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

SOURCES=$(call rwildcard,./,*.c)

all: CFLAGS+= -O3
all: $(OUT)

debug: CFLAGS+= -g -ftrapv -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wcast-qual -Wswitch-default -Wunreachable-code -Wfloat-equal -Wuninitialized -Wignored-qualifiers
debug: $(OUT)

clean:
	rm -f $(OUT)

$(OUT): $(SOURCES)
	$(CC) -I. $(CFLAGS) -o $@ $(LDFLAGS) $^

run:
	./$(OUT)
