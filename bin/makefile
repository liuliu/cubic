include ../lib/config.mk

#CC += -faddress-sanitizer -fno-omit-frame-pointer
LDFLAGS := -L"../lib" -lfreenect $(LDFLAGS)
CFLAGS := -O3 -Wall -I"../lib" $(CFLAGS)

TARGETS = bbview

all: libfreenect.a $(TARGETS)

clean:
	${MAKE} clean -C ../lib ; rm -f *.o $(TARGETS)

$(TARGETS): %: %.o libfreenect.a
	$(CC) -o $@ $< $(LDFLAGS)

libfreenect.a:
	${MAKE} -C ../lib

%.o: %.c ../lib/libfreenect.h ../lib/libfreenect-registration.h
	$(CC) $< -o $@ -c $(CFLAGS)
