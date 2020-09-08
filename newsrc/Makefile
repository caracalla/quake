program_NAME := quake
program_C_SRCS := $(wildcard *.c)
program_OBJS := ${program_C_SRCS:.c=.o}
program_LIBRARIES := m SDL2

CFLAGS := -Wall -O2 -g

LDFLAGS += $(foreach library,$(program_LIBRARIES),-l$(library))

.PHONY: all clean distclean run

all: $(program_NAME)

$(program_NAME): $(program_OBJS)
	$(CC) $(program_OBJS) -o $(program_NAME) $(LDFLAGS)

clean:
	@- $(RM) $(program_NAME)
	@- $(RM) $(program_OBJS)

distclean: clean

run: all
	./$(program_NAME) -nosound -mem 16 -basedir ".." +map e1m1

menu: all
	./$(program_NAME) -nosound -mem 16 -basedir ".."

debug: all
	lldb -- $(program_NAME) -nosound -mem 16 -basedir ".." +map e1m1
