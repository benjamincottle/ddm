SRC	= main.c
OBJ	= main.o
OUT	= ddm
CC = cc
CFLAGS = -g -c -Wall
LFLAGS = -lddcutil

all: $(OBJ)
	$(CC) -g $(OBJ) -o $(OUT) $(LFLAGS)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) $(SRC)

.PHONY: clean
clean:
	rm -f $(OBJ) $(OUT)
