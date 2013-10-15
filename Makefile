# You can put your build options here
-include config.mk

all: libjsnn.a 

libjsnn.a: jsnn.o
	$(AR) rc $@ $^

%.o: %.c jsnn.h
	$(CC) -c $(CFLAGS) $< -o $@

test: jsnn_test
	./jsnn_test

jsnn_test: jsnn_test.o
	$(CC) -L. -ljsnn $< -o $@

jsnn_test.o: jsnn_test.c libjsnn.a

clean:
	rm -f jsnn.o jsnn_test.o
	rm -f jsnn_test
	rm -f libjsnn.a

.PHONY: all clean test

