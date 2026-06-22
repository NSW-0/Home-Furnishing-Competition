CC      = gcc
CFLAGS  = -Wall -Wextra -g -fopenmp
# GL libs must come AFTER object files in the linker command
LDFLAGS = -fopenmp -lglut -lGLU -lGL -lm

TARGET  = furnishing
SRCS    = main.c config.c member.c display.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean run install-deps

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: all
	./$(TARGET) config.txt

# Install dependencies (Ubuntu / WSL)
install-deps:
	sudo apt-get install -y gcc make libomp-dev freeglut3-dev

clean:
	rm -f $(OBJS) $(TARGET)

main.o   : main.c   simulation.h config.h member.h display.h
config.o : config.c config.h
member.o : member.c simulation.h member.h display.h
display.o: display.c display.h simulation.h
