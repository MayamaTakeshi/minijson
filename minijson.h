#ifndef __MINIJSON_H__
#define __MINIJSON_H__

#define JSON_DATATYPE_INVALID 0
#define JSON_DATATYPE_NUMBER 1
#define JSON_DATATYPE_STRING 2
#define JSON_DATATYPE_ARRAY 3
#define JSON_DATATYPE_OBJECT 4
#define JSON_DATATYPE_NULL 5
#define JSON_DATATYPE_TRUE 6
#define JSON_DATATYPE_FALSE 7

typedef struct{
	char* s; /* pointer to the beginning of string (char array) */
	int len; /* string length */
} str;

#define str_init(_string)  {_string, strlen(_string)}

typedef struct {
	str key;
	str val;
	int datatype;
	int visited;
} property_t;

typedef struct {
	char *p; // pointer to current char
	char *end; // pointer to end of string
	int property_collected;
	void *next_step;
	char error[1024];
} minijson_object_parser;

int json_get_datatype(str *s); 

int minijson_parse_object(minijson_object_parser *parser, property_t props[], int *count);
int minijson_find_property_ignorecase(property_t props[], int count, str name, property_t **property);
int minijson_find_property(property_t props[], int count, str name, property_t **property);

void minijson_init_object_parser(minijson_object_parser *parser, str *s);
int minijson_next_property(minijson_object_parser *parser, property_t *property);

int minijson_set_uchar(char *error_buffer, property_t props[], int count, char *name, unsigned char *p);
int minijson_set_ushort(char *error_buffer, property_t props[], int count, char *name, unsigned short *p);
int minijson_set_int(char *error_buffer, property_t props[], int count, char *name, int *p);
int minijson_set_float(char *error_buffer, property_t props[], int count, char *name, float *p);
int minijson_set_uchar_array(char *error_buffer, property_t props[], int count, char *name, unsigned char *p);
int minijson_set_char_array(char *error_buffer, property_t props[], int count, char *name, char *p);

int minijson_strntoi(const char *str, int size);

#endif
