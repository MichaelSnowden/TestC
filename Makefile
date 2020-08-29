.PHONY: build test help install

BUILD="cmake-build-debug"

build:
	mkdir -p $(BUILD); \
	cd $(BUILD); \
	cmake ..; \
	make test;

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