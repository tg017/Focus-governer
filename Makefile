CC = gcc
CFLAGS = -Wall -Wextra -g -I./src
TARGET = governor
OBJS = src/main.o src/monitor.o src/process.o src/utils.o src/policy.o src/enforce.o src/dashboard.o

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) -lncurses

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: clean run