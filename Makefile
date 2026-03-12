CC = gcc
CFLAGS = -Wall -Wextra -g -I./src
TARGET = governor
OBJS = src/main.o src/monitor.o src/process.o src/utils.o

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: clean run