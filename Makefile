CC = gcc
CFLAGS = -I./include -I./src -Wall -Wextra
LDFLAGS =

SRC = src/main.c \
      src/database.c \
      src/json_io.c \
      src/utils.c \
      src/index.c

OBJ = $(SRC:.c=.o)
EXEC = luminix.exe

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $(EXEC) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	del /Q $(EXEC) src\*.o 2>nul || exit 0

run: $(EXEC)
	.\$(EXEC)

test:
	$(CC) $(CFLAGS) -o test_quick.exe src/main.c
	.\test_quick.exe

.PHONY: all clean run test
