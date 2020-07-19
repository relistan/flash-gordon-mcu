CC=clang -g3

all: test

test: main
	rspec spec
