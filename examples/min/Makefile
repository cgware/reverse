all: min

min: main.c
	gcc -nostdlib -c $^
	ld -s -o $@ main.o
