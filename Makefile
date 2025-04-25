CC = gcc
CFLAGS = -Wall -Wextra -Werror
TARGET = sshell
SRC = sshell.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) *.o