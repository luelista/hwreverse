.PHONY: all clean
all: fwbuilder spiparser
clean:
	rm fwbuilder spiparser

fwbuilder: fwbuilder.c
	gcc -o $@ $<

spiparser: spiparser.c
	gcc -o $@ $<

