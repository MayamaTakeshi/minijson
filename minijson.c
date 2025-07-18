#include "minijson.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h> /* for strtod */


int char2int(char c) {
	if(c >= '0' && c <= '9') {
		return c - '0';
	} else if(c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	} else if(c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return 0;
}

int json_is_number(str *s) {
/* 
http://cjihrig.com/blog/json-overview/

JSON numbers must not have leading zeros,
and must have at least one digit following a decimal point.
Due to the restriction on leading zeros, JSON supports base 10 numbers only (octal and hexadecimal formats both require a leading zero). 
*/
	int i = 0;
	int dot_seen = 0;
	if(s->len <= 0)  return 0;

	if(s->s[0] == '.') return 0; /* cannot start with dot */

	if(s->len >= 2 && s->s[0] == '0') return 0; /* leading zero not allowed */

	if(s->len >= 2 && s->s[0] == '-') {
		i += 1; 
	}

	for( ; i < s->len; ++i){
		if(dot_seen) {
			if(!isdigit(s->s[i])) return 0;
		} else {
			if(!isdigit(s->s[i]) && s->s[i] != '.') return 0;
			if(s->s[i] == '.') {
				dot_seen = 1;
			}
		}
		
	}
	return 1;
}

int json_get_datatype(str *s) {
	if(json_is_number(s)) return JSON_DATATYPE_NUMBER;

	if(s->len == 4 && strncmp(s->s, "null", 4) == 0) return JSON_DATATYPE_NULL;

	if(s->len == 4 && strncmp(s->s, "true", 4) == 0) return JSON_DATATYPE_TRUE;

	if(s->len == 5 && strncmp(s->s, "false", 5) == 0) return JSON_DATATYPE_FALSE;

	return JSON_DATATYPE_INVALID;
}

int minijson_strntoi(const char *str, int size) {
	int sign = 1;
	if(size > 0 && str[0] == '-') {
		sign = -1;
		str++;
		size -= 1;
	}
	size -= 1;
        int val = 0;
        while(size >= 0) {
                val += (*str++ - '0') * pow(10,size);
                --size;
        }
        return val * sign;
}

int is_byte_array_string(str *s) {
        /* byte array is a string in the format "01020304" where each 2 chars correspond to 1 byte */
	int i;
        if(s->len % 2) return 0; /* must be even */

        for(i=0 ; i > s->len ; ++i) {
                if(!isxdigit(s->s[i])) return 0;
        }
        return 1;
}

void minijson_set_error(char *buff, char* format, ...);

#define SET_ERROR(buff, format, ...) minijson_set_error(buff, "%s:%i " format, __FILE__, __LINE__, __VA_ARGS__)

void minijson_init_parser(minijson_object_parser *parser, str *s);

/* signature of parsing functions of our FSM */
typedef void (*parsing_func) (minijson_object_parser *parser, property_t *property);

#define PARSING_FUNC(func) void func (minijson_object_parser *parser, property_t *property)
PARSING_FUNC(fsm_obj_find_open_bracket);
PARSING_FUNC(fsm_obj_next_key);
PARSING_FUNC(fsm_obj_find_colon);
PARSING_FUNC(fsm_obj_next_val);
PARSING_FUNC(fsm_obj_find_comma);
PARSING_FUNC(fsm_obj_no_garbage);

PARSING_FUNC(fsm_obj_find_open_bracket) {
#ifdef DEBUG
	printf("Entering fsm_obj_find_open_bracket with '%.*s'\n", parser->end - parser->p, parser->p);
#endif
	char *p = parser->p;
	while(p != parser->end) { 
		if(*p == '{') {
			++p;
			parser->p = p;
			parser->next_step = fsm_obj_next_key;
			return;
		}

		if(*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
			++p;
			continue;
		}

		SET_ERROR(parser->error, "fsm_obj_find_open_bracket: unexpected char '%c' while searching for '{'", *p);	
		return;
	}
	SET_ERROR(parser->error, "fsm_obj_find_open_bracket: Unexpected end of string%s", ""); /* variadic macros require at least one argument to be passed to the ellipsis*/
	return;
}

PARSING_FUNC(fsm_obj_next_key) {
#ifdef DEBUG
	printf("Entering fsm_obj_next_key with '%.*s'\n", parser->end - parser->p, parser->p);
#endif
	char *p = parser->p;
	while(p != parser->end) {
		if(*p == '}') {
			/* found end of json */
			++p;
			parser->p = p;
			parser->next_step = fsm_obj_no_garbage;
			return;
		}

		if(*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
			++p;
			 continue;
		}

		if(*p == '"') {
			++p;
			if(p == parser->end) { 
				SET_ERROR(parser->error, "fsm_obj_next_key: unexpected end of string while searching for start of key '\"'%s", "");
				return;
			}
			property->key.s = p;
			while(*p != '"') {
				if(p == parser->end) {
					SET_ERROR(parser->error, "fsm_obj_next_key: unexpected end of string while searching for closing '\"'%s", "");
					return;
				}
				++p;
			}
			property->key.len = p - property->key.s;
			if(property->key.len <= 0) {
				SET_ERROR(parser->error, "fsm_obj_next_key: invalid zero-length key%s", "");
				return;
			}
			++p;
			parser->p = p;
			parser->next_step = fsm_obj_find_colon;
			return;
		}
		SET_ERROR(parser->error, "fsm_obj_next_key: unexpected char '%c' while searching for opening '\"'", *p);
		return;
	}		
	SET_ERROR(parser->error, "fsm_obj_next_key: Unexpected end of string%s", "");
	return;
}

PARSING_FUNC(fsm_obj_find_colon) {
#ifdef DEBUG
	printf("Entering fsm_obj_find_colon with '%.*s'\n", parser->end - parser->p, parser->p);
#endif
	char *p = parser->p;
	while(p != parser->end) {
		if(*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
			++p;
			continue;
		}

		if(*p == ':') {
			++p;
			parser->p = p;
			parser->next_step = fsm_obj_next_val;
			return;
		}
			
		SET_ERROR(parser->error, "fsm_obj_find_colon: unexpected char '%c' while searching for ':'", *p);	
		return;
	}
	SET_ERROR(parser->error, "fsm_obj_find_colon: Unexpected end of string%s", "");
	return;
}

PARSING_FUNC(fsm_obj_next_val) {
#ifdef DEBUG
	printf("Entering fsm_obj_next_val with '%.*s'\n", parser->end - parser->p, parser->p);
#endif
	char *p = parser->p;
	while(p != parser->end) {
		if(*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
			++p;
			 continue;
		}

		if(*p == '}' || *p == ']' || *p == '(' || *p == ')') {
			SET_ERROR(parser->error, "fsm_obj_next_val: unexpected '%c' while waiting for start of value", *p);	
			return;
		}

		if(*p == '"') {
			/* value is double-quoted */
			++p;
			if(p == parser->end) { 
				SET_ERROR(parser->error, "fsm_obj_next_val: unexpected end of string while searching for start of quoted value'%s", "");
				return;
			}

			property->val.s = p;
			while(*p != '"') {
				if(p == parser->end) {
					SET_ERROR(parser->error, "fsm_obj_next_val: unexpected end of string while searching for closing '\"'%s", "");
					return;
				}
				++p;
			}
			property->val.len = p - property->val.s;
			property->datatype = JSON_DATATYPE_STRING;
			++p;
			parser->p = p;
			parser->property_collected = 1;
			parser->next_step = fsm_obj_find_comma;
			return;
		} else {
			/* value is unquoted */
			if(*p != '{' && *p != '[') {
				/* number or null or true or false */
				property->val.s = p;
				while(*p != ' ' && *p != '\n' && *p != '\r' && *p != '\t' && *p != ',' && *p != '}') {
					++p;
					if(p == parser->end) {
						SET_ERROR(parser->error, "fsm_obj_next_val: unexpected end of string while collecting number|null|true|false string%s", "");
						return;
					}
				}
				property->val.len = p - property->val.s;
				int datatype = json_get_datatype(&property->val);
				if(datatype == JSON_DATATYPE_INVALID) {
					SET_ERROR(parser->error, "fsm_obj_next_val: invalid string for number/constant %*.s", property->val.len, property->val.s);
					return;
				}
				property->datatype = datatype;
				if(*p == ',') {
					++p;
					parser->p = p;
					parser->property_collected = 1;
					parser->next_step = fsm_obj_next_key;
					return;
				} else if(*p == '}') {
					++p;
					parser->p = p;
					parser->property_collected = 1;
					parser->next_step = fsm_obj_no_garbage;
					return;
				} else {
					++p;
					parser->p = p;
					parser->property_collected = 1;
					parser->next_step = fsm_obj_find_comma;
					return;
				}
			} else {
				/* { or [ */
				int datatype;
				char closing_char;
				char stack[1024];
				int idx = 0;
				if(*p == '{') {
					datatype = JSON_DATATYPE_OBJECT;
					closing_char = '}';
				} else {
					closing_char = ']';
					datatype = JSON_DATATYPE_ARRAY;
				}
				stack[idx] = closing_char;
				
				property->datatype = datatype;
				property->val.s = p;
				while(1) {
					++p;
					if(p == parser->end) {
						SET_ERROR(parser->error, "fsm_obj_next_val: unexpected end of string while collecting object|array string%s", "");
						return;
					}
					if(*p == '"') {	
						if(idx == 0) {
							idx++;
							stack[idx] = '"';
						} else {
							if(stack[idx] == '"') {
								idx--;
							} else {
								idx++;
								stack[idx] = '"';
							}
						}
					} else if (*p == '{') {
						idx++;
						stack[idx] = '}';
					} else if (*p == '[') {
						idx++;
						stack[idx] = ']';
					} else if(*p == '}' || *p == ']') {
						if(*p == stack[idx]) {
							if(idx == 0) {
								++p;
								parser->p = p;
								property->val.len = p - property->val.s;
								parser->property_collected = 1;
								parser->next_step = fsm_obj_find_comma;
								return;
							} else {
								idx--;
							}
						} else {
							SET_ERROR(parser->error, "fsm_obj_next_val: malformed value for %.*s", property->key.len, property->key.s);
							return;
						}
					}
				}
			}
		}
	}
	SET_ERROR(parser->error, "fsm_obj_next_val: Unexpected end of string%s", "");
	return;
}

PARSING_FUNC(fsm_obj_find_comma) {
#ifdef DEBUG
	printf("Entering fsm_obj_find_comma with '%.*s'\n", parser->end - parser->p, parser->p);
#endif
	char *p = parser->p;
	while(p != parser->end) {
		if(*p != ' ' && *p != '\n' && *p != '\r' && *p != '\t' && *p != ',' && *p != '}') {
			SET_ERROR(parser->error, "fsm_obj_find_comma: unexpected '%c' while waiting for ','", *p);
			return;
		}

		if(*p == ',') {
			++p;
			parser->p = p;
			parser->next_step = fsm_obj_next_key;
			return;
		}

		if(*p == '}') {
			/* found end of json */
			++p;
			parser->p = p;
			parser->next_step = fsm_obj_no_garbage;
			return;
		}

		++p;
	}
	SET_ERROR(parser->error, "fsm_obj_find_comma: Unexpected end of string%s", "");
	return;
}

PARSING_FUNC(fsm_obj_no_garbage) {
#ifdef DEBUG
	printf("Entering fsm_obj_no_garbage with '%.*s'\n", parser->end - parser->p, parser->p);
#endif
	char *p = parser->p;
	while(p != parser->end) {
		if(*p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') {
			SET_ERROR(parser->error, "fsm_obj_no_garbage: garbage '%s' after closing bracket", p);
			return;
		}
		++p;
	}
	return;
}

void minijson_init_object_parser(minijson_object_parser *parser, str *s) {
	parser->p = s->s;
	parser->end = parser->p + s->len;
	parser->next_step = (parsing_func)fsm_obj_find_open_bracket;
	parser->error[0] = 0;
}

/* Returns: 0 = error, 1 = success */
int minijson_parse_object(minijson_object_parser *parser, property_t props[], int *count) {
	int max_props = *count;
	property_t property;
	int has_more = 1;
	parsing_func next_step;

	*count = 0;

	property.visited = 0;
	while(parser->next_step) {
		parser->property_collected = 0;
		next_step = (parsing_func)parser->next_step;
		parser->next_step = 0;
		next_step(parser, &property);
		if(parser->property_collected) {
			(*count)++;
			if(*count > max_props) { 
				SET_ERROR(parser->error, "minijson_parse_object: no space in array for new key (count=%i)", *count);
				return 0;
			}
			props[*count-1] = property;
		}
	}

	if(parser->error[0] != 0) {
		return 0;
	} 

	/* success */
	return 1;
}

/* Returns: 1 = got property, 0 = haven't got property or error */
int minijson_next_property(minijson_object_parser *parser, property_t *property) {
	parsing_func next_step;

	if(!parser->next_step) {
		/* parsing already finished */
		return 0;
	}
	
	while(parser->next_step) {
		parser->property_collected = 0;
		next_step = (parsing_func)parser->next_step;
		parser->next_step = 0;
		next_step(parser, property);

		if(parser->error[0] != 0) {
			return 0;
		}

		if(parser->property_collected) {
			return 1;
		}
	}
	return 0;
}

/* Returns: 1 = got property, 0 = haven't got property */
int minijson_find_property_ignorecase(property_t props[], int count, str name, property_t **property) {
	int i;
	for(i=0;i<count;++i) {
		if(!props[i].visited && (name.len == props[i].key.len) && strncasecmp(name.s, props[i].key.s, name.len) == 0) {
			props[i].visited = 1;
			*property = &props[i];
			return 1;
		}
	}
	return 0;
}

/* Returns: 1 = got property, 0 = haven't got property */
int minijson_find_property(property_t props[], int count, str name, property_t **property) {
	int i;
	for(i=0;i<count;++i) {
		if(!props[i].visited && (name.len == props[i].key.len) && strncmp(name.s, props[i].key.s, name.len) == 0) {
			props[i].visited = 1;
			*property = &props[i];
			return 1;
		}
	}
	return 0;
}

void minijson_set_error(char *buff, char* format, ...) {
	va_list args;
	va_start(args, format);
	va_end(args);
	vsprintf(buff, format, args);
}

int minijson_set_uchar(char *error_buffer, property_t props[], int count, char *name, unsigned char *p) {
        property_t *prop;
        if(!minijson_find_property_ignorecase(props, count, (str)str_init(name), &prop)) {
                sprintf(error_buffer, "Expected property '%s' not present", name);
                return 0;
        }

        *p = minijson_strntoi(prop->val.s, prop->val.len);
        return 1;
}

int minijson_set_ushort(char *error_buffer, property_t props[], int count, char *name, unsigned short *p) {
        property_t *prop;
        if(!minijson_find_property_ignorecase(props, count, (str)str_init(name), &prop)) {
                sprintf(error_buffer, "Expected property '%s' not present", name);
                return 0;
        }

        *p = minijson_strntoi(prop->val.s, prop->val.len);
        return 1;
}

int minijson_set_int(char *error_buffer, property_t props[], int count, char *name, int *p) {
        property_t *prop;
        if(!minijson_find_property_ignorecase(props, count, (str)str_init(name), &prop)) {
                sprintf(error_buffer, "Expected property '%s' not present", name);
                return 0;
        }

        *p = minijson_strntoi(prop->val.s, prop->val.len);
        return 1;
}

int minijson_set_float(char *error_buffer, property_t props[], int count, char *name, float *p) {
        property_t *prop;
        if(!minijson_find_property_ignorecase(props, count, (str)str_init(name), &prop)) {
                sprintf(error_buffer, "Expected property '%s' not present", name);
                return 0;
        }

        *p = strtod(prop->val.s, NULL); /* the function stops at the first non numeric char */
		printf("set_float p=%f\n", *p);
        return 1;
}

int minijson_set_uchar_array(char *error_buffer, property_t props[], int count, char *name, unsigned char *p) {
	int i;
        property_t *prop;
        if(!minijson_find_property_ignorecase(props, count, (str)str_init(name), &prop)) {
                sprintf(error_buffer, "Expected property '%s' not present", name);
                return 0;
        }

        if(!is_byte_array_string(&prop->val)) {
                sprintf(error_buffer, "Invalid format for property '%s' value (\"%.*s\") : it must be byte array string", name, prop->val.len, prop->val.s);
                return 0;
        }

        for(i=0 ; i < prop->val.len/2 ; ++i) {
		char *nibble = &prop->val.s[2*i];
		*p = char2int(*nibble) << 4;
		*p += char2int(*(++nibble));
		p++;
        }
        return 1;
}

int minijson_set_char_array(char *error_buffer, property_t props[], int count, char *name, char *p) {
	int i;
        property_t *prop;
        if(!minijson_find_property_ignorecase(props, count, (str)str_init(name), &prop)) {
                sprintf(error_buffer, "Expected property '%s' not present", name);
                return 0;
        }

        if(!is_byte_array_string(&prop->val)) {
                sprintf(error_buffer, "Invalid format for property '%s' value (\"%.*s\") : it must be byte array string", name, prop->val.len, prop->val.s);
                return 0;
        }

        for(i=0 ; i < prop->val.len/2 ; ++i) {
		char *nibble = &prop->val.s[2*i];
		*p = char2int(*nibble) << 4;
		*p += char2int(*(++nibble));
		p++;
        }
        return 1;
}

