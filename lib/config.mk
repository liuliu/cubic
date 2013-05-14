CC := clang
AR := ar
CFLAGS := -msse2 -I"."
LDFLAGS := -lm -lusb-1.0 -lpthread -lglut -lGL -lGLU
