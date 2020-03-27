.PHONY: all clean

all:
	gcc -Wall -o asm asm.c `pkg-config --cflags --libs mono-2`
clean:
	-@rm -r asm output/
