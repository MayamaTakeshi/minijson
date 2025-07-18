all: minijson_test

static_lib: minijson.c minijson.h
	gcc -fPIC -g -c minijson.c -o minijson.o
	ar rcs libminijson.a minijson.o  

minijson_test: static_lib minijson_test.c
	gcc -g minijson_test.c -L. -lminijson -lm -o minijson_test

clean:
	rm -f *.a *.o minijson_test
