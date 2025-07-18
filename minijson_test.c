#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>

#include <string.h>


#include "minijson.h"

#define SLEEPMODE_USLEEP 1
#define SLEEPMODE_NANOSLEEP 2
#define SLEEPMODE_SELECT 3

#define PARSEMODE_FULL 1
#define PARSEMODE_PULL 2

#define MAX_PROPERTIES 10

void usage(char *app_name) {
	printf("Usage:\n");
	printf("args: %s iterations parsemode sleepmode json_string\n", app_name);
	printf("ex:   %s 10000000 full usleep '{\"key1\": 1}'\n", app_name);
	printf("Details:\n");
	printf("           parsemode = full|pull\n");
	printf("           sleepmode = usleep|nanosleep|select\n");
}

void test_full(char *s) {
	minijson_object_parser parser;
	property_t properties[MAX_PROPERTIES];
	int count = MAX_PROPERTIES;
	int suc = 0;
	str ss;
	ss.s = s;
	ss.len = strlen(s);

	property_t *property;

	minijson_init_object_parser(&parser, &ss);

	int i;
	if(minijson_parse_object(&parser, properties, &count)) {
		printf("Success count=%i\n", count);
		for(i=0; i<count ; i++) {
			property_t *property = &properties[i];
			printf("%.*s => %.*s (datatype=%i)\n", property->key.len, property->key.s, property->val.len, property->val.s, property->datatype);
		}
		str key = {"d", 1};
		if(minijson_find_property(properties, count, (str)str_init("d"), &property)) {
			printf("Property d found: d => %.*s\n", property->val.len, property->val.s);
		} else {
			printf("Property d not found\n");
		}
		suc++;
	} else {
		printf("ERROR: %s\n", parser.error);
	}
}

void test_pull(char *s) {
	minijson_object_parser parser;
	property_t property;
	str ss;
	ss.s = s;
	ss.len = strlen(s);

	minijson_init_object_parser(&parser, &ss);

	while(minijson_next_property(&parser, &property)) {
		printf("%.*s => %.*s (datatype=%i)\n", property.key.len, property.key.s, property.val.len, property.val.s, property.datatype);
	}

	if(parser.error[0] != 0) {
		printf("ERROR: %s\n", parser.error);
		return;
	}
}

void test_minijson_set_funcs() {
	int i;
	minijson_object_parser parser;
	property_t props[MAX_PROPERTIES];
	int count = MAX_PROPERTIES;
	int suc = 0;
	char error[1024];

	unsigned char ba[1024];
	memset(&ba[i], 7, sizeof(ba));

	char json[] = "{\"byte_array\": \"000102FAFBfcfe0a0b0c0d0e0f0A0B0C0D0E0F0101010800010f0002121501020408831021436587090f0a0703151865658787\", \"eca\": \"abc\", \"the_uchar\": 3, \"the_int\": -123}";
	printf("json = %s\n", json);
	str s;
	s.s = json;
	s.len = strlen(json);
	
	minijson_init_object_parser(&parser, &s);

	unsigned char the_uchar;

	int the_int;

	if(!minijson_parse_object(&parser, props, &count)) {
		printf("test_minijson_set_funcs: minijson_parse_object failed: %s\n", parser.error);
		return;
	}

	if(!minijson_set_uchar_array(error, props, count, "byte_array", ba)) {
		printf("test_minijson_set_funcs: minijson_set_uchar_array failed\n");
	}

	if(!minijson_set_int(error, props, count, "the_int", &the_int)) {
		printf("test_minijson_set_funcs: minijson_set_int failed\n");
	}

	if(!minijson_set_uchar(error, props, count, "the_uchar", &the_uchar)) {
		printf("test_minijson_set_funcs: minijson_set_uchar failed\n");
	}

	printf("byte_array: %x %x %x %x %x %x %x %x %x\n", ba[0], ba[1], ba[2], ba[3], ba[4], ba[5], ba[6], ba[7], ba[8]);
	printf("the_int: %i\n", the_int);
	printf("the_uchar: %u\n", the_uchar);
}

int main(int argc, char *argv[]) {
	// char default_str[] = " { \n\"key1\":1, \"key2\": \"val2\", \"key3\" : 3 , \"key4\"\t:4,\t\"key5\":\"val5\"}";
	char *s;
	int iterations;
	int parsemode = 0;
	int sleepmode = 0;

	test_minijson_set_funcs();

	if(argc != 5) {
		usage(argv[0]);
		return 1;
	}

	iterations = atoi(argv[1]);

	if(strcmp(argv[2],"full") == 0) 
		parsemode = PARSEMODE_FULL;
	else if(strcmp(argv[2],"pull") == 0)
		parsemode = PARSEMODE_PULL;

	if(!parsemode) {
		printf("Invalid parsemode\n");
		usage(argv[0]);
		return 1;
	}

	if(strcmp(argv[3],"usleep") == 0) 
		sleepmode = SLEEPMODE_USLEEP;
	else if(strcmp(argv[3],"nanosleep") == 0)
		sleepmode = SLEEPMODE_NANOSLEEP;
	else if(strcmp(argv[3],"select") == 0)
		sleepmode = SLEEPMODE_SELECT;

	if(!sleepmode) {
		printf("Invalid sleepmode\n");
		usage(argv[0]);
		return 1;
	}
	
	s = argv[4];

	printf("Parsing %s\n",s);
	while(iterations-- > 0) {
		if(parsemode == PARSEMODE_FULL) 
			test_full(s);
		else 
			test_pull(s);
		

		if(sleepmode == SLEEPMODE_USLEEP) {
			usleep(1);
		} else if(sleepmode == SLEEPMODE_NANOSLEEP) {

			struct timespec tim;
			tim.tv_sec = 0;
			tim.tv_nsec = 1;
			if(nanosleep(&tim, NULL) < 0) {
				printf("nanosleep call failed\n");
				return 1;
			}
		} else if(sleepmode == SLEEPMODE_SELECT) {
			struct timeval t = {0, 1};
			select(0, NULL, NULL, NULL, &t);
		}

		/*
		if(suc % 1000000 == 0) {
			printf("suc=%d\n", suc);
		}
		*/
	}

	return 0;
}
