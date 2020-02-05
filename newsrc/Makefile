program_NAME := quake
program_C_SRCS := $(wildcard *.c)
program_OBJS := ${program_C_SRCS:.c=.o}
program_LIBRARIES := m SDL2

CFLAGS := -Wall -O2

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
	./$(program_NAME) -nosound -mem 16