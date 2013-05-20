include ../lib/config.mk

#CC += -faddress-sanitizer -fno-omit-frame-pointer
LDFLAGS := -L"../lib" -lcubic $(LDFLAGS)
CFLAGS := -O3 -Wall -I"../lib" $(CFLAGS)

TARGETS = view

all: libcubic.a $(TARGETS)

clean:
	${MAKE} clean -C ../lib ; rm -f *.o $(TARGETS)

$(TARGETS): %: %.o libcubic.a
	$(CC) -o $@ $< $(LDFLAGS)

libcubic.a:
	${MAKE} -C ../lib

%.o: %.c ../lib/cubic.h
	$(CC) $< -o $@ -c $(CFLAGS)
