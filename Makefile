CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -D_DEFAULT_SOURCE -Iinclude

MODULES = src/collector.c src/parser.c src/metrics.c src/filter_sort.c \
          src/ui.c src/refresh.c src/error_handler.c

all: procmon

procmon: $(MODULES) src/main.c
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_system: $(MODULES) tests/test_system.c
	$(CC) $(CFLAGS) $^ -o $@ -lm

test: test_system
	./test_system

clean:
	rm -f procmon test_system

.PHONY: all test clean