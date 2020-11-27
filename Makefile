.PHONY: build test help install

BUILD="cmake-build-debug"

build:
	mkdir -p $(BUILD); \
	cd $(BUILD); \
	cmake -DCMAKE_BUILD_TYPE=Debug ..; \
	make VERBOSE=1 test;

# Run with --nofork so that `make test` shows what running example test would
# look like, not the actual test for this repo.
ARGS=--nofork

test: build
	cd $(BUILD); \
	./test/test $(ARGS)

help: build
	cd $(BUILD); \
	./test/test --help

install: build; \
	cd $(BUILD); \
	make install