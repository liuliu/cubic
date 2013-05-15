include config.mk

#CC += -faddress-sanitizer -fno-omit-frame-pointer
CFLAGS := -O3 -ffast-math -Wall $(CFLAGS)# -fprofile-arcs -ftest-coverage

.PHONY: all clean

all: libcubic.a

clean:
	rm -f *.o libfreenect.a

libcubic.a: cubic.o cameras.o core.o registration.o tilt.o usb_libusb10.o
	$(AR) rcs $@ $^

%.o: %.c cubic.h libfreenect.h freenect_internal.h libfreenect-registration.h registration.h usb_libusb10.h
	$(CC) $< -o $@ -c $(CFLAGS)
