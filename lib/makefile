include config.mk

#CC += -faddress-sanitizer -fno-omit-frame-pointer
CFLAGS := -O3 -ffast-math -Wall $(CFLAGS)# -fprofile-arcs -ftest-coverage

.PHONY: all clean

all: libfreenect.a

clean:
	rm -f *.o libfreenect.a

libfreenect.a: cameras.o core.o registration.o tilt.o usb_libusb10.o
	$(AR) rcs $@ $^

%.o: %.c libfreenect.h freenect_internal.h libfreenect-registration.h registration.h usb_libusb10.h
	$(CC) $< -o $@ -c $(CFLAGS)
