CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lm

# Compiles the demo and runs it so `make test` exercises the C library
# end to end (parse JSON -> render line/candle -> print).
test: main.c
	mkdir -p build/bin
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o build/bin/test
	./build/bin/test

# Builds the Python extension in place and runs the unit test suite.
test-py:
	python3 setup.py build_ext --inplace
	python3 -m unittest discover -s tests -v

.PHONY: test test-py clean
clean:
	rm -rf build/ ccharts/*.so ccharts/__pycache__ tests/__pycache__