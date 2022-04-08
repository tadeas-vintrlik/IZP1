CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -Werror
FILE=sheet
all: sheet.c
	$(CC) $(CFLAGS) -o $(FILE) $(FILE).c
debug: sheet.c
	$(CC) $(CFLAGS) -g -o $(FILE) $(FILE).c
