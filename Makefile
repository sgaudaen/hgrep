CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE
LDFLAGS = -lncurses

SRC = src/hgrep.c
OBJ = $(SRC:.c=.o)
TARGET = hgrep

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

install: $(TARGET)
	install -D -m 755 $(TARGET) $(HOME)/.local/bin/$(TARGET)

uninstall:
	rm -f $(HOME)/.local/bin/$(TARGET)
