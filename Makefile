httpget: httpget.c
	gcc httpget.c -o httpget -L./libwebsockets/build/lib -lwebsockets


clean:
	rm *.o httpget