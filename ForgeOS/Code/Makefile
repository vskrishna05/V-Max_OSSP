CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu11
TARGET = monitor
SRCS = main.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) monitor_log.txt

.PHONY: all clean run
