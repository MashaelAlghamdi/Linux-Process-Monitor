CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -D_DEFAULT_SOURCE -Iinclude

MODULES = src/collector.c src/parser.c src/metrics.c src/filter_sort.c \
          src/ui.c src/refresh.c src/error_handler.c
HEADERS = $(wildcard include/*.h)
TESTS   = test_collector test_metrics test_system test_ui

all: procmon

procmon: $(MODULES) src/main.c $(HEADERS)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ -lm

test_collector: src/collector.c src/parser.c tests/test_collector.c $(HEADERS)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

test_metrics: src/metrics.c tests/test_metrics.c $(HEADERS)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ -lm

test_system: $(MODULES) tests/test_system.c $(HEADERS)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ -lm

test_ui: src/ui.c src/filter_sort.c tests/test_ui.c $(HEADERS)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

test: $(TESTS)
	@for t in $(TESTS); do \
	    echo "=== $$t ==="; \
	    ./$$t || { echo "*** $$t FAILED ***"; exit 1; }; \
	done; \
	echo; echo "All $(words $(TESTS)) test programs passed."

clean:
	rm -f procmon $(TESTS)

.PHONY: all test clean
