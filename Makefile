sockio-test: sockio-test.c
	gcc -g  sockio-test.c -o sockio-test -L./libwebsockets/build/lib -lwebsockets


clean:
	rm -f *.o sockio-test