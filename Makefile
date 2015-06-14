
CC=gcc
CFLAGS=
ROOT_DIR = $(CURDIR)
TARGET=main


ifeq ($(BUILD),)
	BUILD=DEBUG
endif

ifeq ($(BUILD),Release)
	CFLAGS += -O2 -g0
else 
	CFLAGS += -O0 -g3
endif


SRCS = 	main.c\
		malloc.c\
		wtree.c

OBJS = $(SRCS:%.c=%.o)

INC = $(ROOT_DIR)

STATIC_LIB = libcdsl.a


CFLAGS += $(INC:%=-I%)

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS)  -o $@ $(OBJS) $(STATIC_LIB)
	
%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $< $(STATIC_LIB)
	
	
clean :
	rm -rf $(OBJS) $(TARGET)
