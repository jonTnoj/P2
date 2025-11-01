CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = spell
SRC = spell.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) *.o

run: $(TARGET)
	./$(TARGET) dict.txt test.txt