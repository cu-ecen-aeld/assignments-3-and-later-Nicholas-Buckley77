#Nicholas Buckley Assignment 5 AESD makefile

#The skeleton of this make file is from chat GPT-4 with the prompt 
#"a make file that has the defualt target writer and a clean target which removes all .o files and the app with support for CROSS_COMPILE variable"

# Added the cross_compile flag for compiling with Arm x86 if the argument is added
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

SRCS = aesdsocket.c

# Pair objects with the source c files
OBJS = $(SRCS:.c=.o)
APP = aesdsocket

# all and clean are just rules / targets not files
.PHONY: all clean

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(APP)
