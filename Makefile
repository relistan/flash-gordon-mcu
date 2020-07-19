CC=clang -g3

all: test

encoder:
	go build -o encoder main.go

test: main encoder
	bundle exec rspec --format=documentation spec
