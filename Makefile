CC = D:\MinGW-w64\mingw32\bin\gcc.exe
CFLAGS = -I./include -I./src -w
LDFLAGS =

SRC = src/main.c \
      src/database.c \
      src/json_io.c \
      src/utils.c \
      src/index.c \
      src/backup.c \
      src/cli.c \
      src/export_import.c \
      src/parser.c \
      src/query_engine.c \
      src/security.c \
      src/storage.c \
      src/transaction.c

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
