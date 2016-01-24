NAME = nlmon
CC = gcc
LD = gcc
CFLAGS = -g -Wall
LDFLAGS =
LDLIBS = -lpthread -lrt -lm

# hard enable ncurses 
CONFIG_NCURSES = 1

CONFIG_NCURSES ?= 0

ifeq ($(CONFIG_NCURSES), 1)
	LDLIBS += -ltinfo -lncurses
	CFLAGS += -DCONFIG_NCURSES
endif

OBJS = bitmap.o proc_events.o nlmon.o hash.o out_csv.o out_stdout.o \
       out_nop.o data_cpu.o data_memory.o cache.o rbtree.o

ifeq ($(CONFIG_NCURSES), 1)
	OBJS += out_ncurses.o
endif

all: $(OBJS) $(NAME)

%.o: %.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $< -o $@

%: %.o $(OBJS) Makefile
	$(CROSS_COMPILE)$(LD) $(LDFLAGS) $(OBJS) -o $(NAME) $(LDLIBS)

clean:
	rm -f $(NAME) *.o tags

tags:
	ctags *.h *.c

.PHONY: clean tags
