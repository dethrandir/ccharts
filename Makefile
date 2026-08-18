CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lm

test: main.c
	mkdir -p build/bin
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o build/bin/test

.PHONY: clean
clean:
	rm -rf build/
