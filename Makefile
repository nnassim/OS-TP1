# Makefile pour biceps v0.1

CC      = gcc
CFLAGS  = -Wall
LDFLAGS = -lreadline

TARGET  = biceps
SRC     = biceps.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)