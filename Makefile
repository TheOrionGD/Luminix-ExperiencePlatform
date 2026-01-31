CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
LDFLAGS = -lm
TARGET = luminix
SRC_DIR = src
SRC = $(SRC_DIR)/main.c $(SRC_DIR)/database.c $(SRC_DIR)/json_io.c $(SRC_DIR)/utils.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET) database.json

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	./$(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/
	echo "Luminix installed successfully!"

uninstall:
	rm -f /usr/local/bin/$(TARGET)
	echo "Luminix uninstalled!"