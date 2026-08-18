CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lm

test: main.c
	mkdir -p build/bin
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o build/bin/test

test-py:
	python3 setup.py build_ext --inplace
	python3 -m unittest tests.test_python_api -v

.PHONY: clean
clean:
	rm -rf build/ ccharts/*.so ccharts/__pycache__
