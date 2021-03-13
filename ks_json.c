#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "ks_json.h"

enum
{
	KSTATE_START_OBJECT,
	KSTATE_KEY_IN_OBJECT,
	KSTATE_KEY_STRING,
	KSTATE_KEY_SEPARATOR,
	KSTATE_VAL_IN_OBJECT,
	KSTATE_VAL_STRING,
	KSTATE_VAL_OTHER,
	KSTATE_VAL_END,
	KSTATE_TERMINATION,

	KSTATE_FINISHED
};

enum
{
	JTYPE_STRING,
	JTYPE_OTHER // TODO: more types
};

//
// funcs

static uint8_t *skip_whitespace(uint8_t *ptr, uint8_t *end)
{
	while(ptr < end)
	{
		register uint8_t tmp = *ptr;
		if(tmp != ' ' && tmp != '\t' && tmp != '\n' && tmp != '\r')
			break;
		ptr++;
	}
	return ptr;
}

static uint8_t *get_string(kgstruct_json_t *ks, uint8_t *ptr, uint8_t *end)
{
	while(ptr < end)
	{
		register uint8_t tmp = *ptr;
		if(!ks->escaped)
		{
			if(tmp == '"')
				break;
			if(tmp == '\\')
			{
				ks->escaped = 1;
				ptr++;
				continue;
			}
		} else
			ks->escaped = 0;
		*ks->ptr++ = tmp;
		if(ks->ptr == ks->str + KS_JSON_MAX_STRING_LENGTH)
			return NULL;
		ptr++;
	}
	return ptr;
}

//
// API

int ks_json_parse(kgstruct_json_t *ks, uint8_t *buff, uint32_t length)
{
	uint8_t *end = buff + length;

	switch(ks->state)
	{
		case KSTATE_FINISHED:
//			printf("KSTATE_FINISHED\n");
			return ks->error;
		case KSTATE_KEY_IN_OBJECT:
//			printf("KSTATE_KEY_IN_OBJECT\n");
			goto continue_key_in_object;
		case KSTATE_KEY_STRING:
//			printf("KSTATE_KEY_STRING\n");
			goto continue_key_string;
		case KSTATE_KEY_SEPARATOR:
//			printf("KSTATE_KEY_SEPARATOR\n");
			goto continue_key_separator;
		case KSTATE_VAL_IN_OBJECT:
//			printf("KSTATE_VAL_IN_OBJECT\n");
			goto continue_val_in_object;
		case KSTATE_VAL_STRING:
//			printf("KSTATE_VAL_STRING\n");
			goto continue_val_string;
		case KSTATE_VAL_OTHER:
//			printf("KSTATE_VAL_OTHER\n");
			goto continue_val_other;
		case KSTATE_VAL_END:
//			printf("KSTATE_VAL_END\n");
			goto continue_val_end;
		case KSTATE_TERMINATION:
//			printf("KSTATE_TERMINATION\n");
			goto continue_termination;
	}

	while(1)
	{
		// skip whitespaces
		buff = skip_whitespace(buff, end);
		if(buff >= end)
		{
			ks->state = KSTATE_START_OBJECT;
			return KS_JSON_MORE_DATA;
		}

		// need object
		if(*buff != '{')
		{
			ks->error = KS_JSON_EXPECTED_OBJECT;
			goto finished;
		}
		buff++;

continue_key_in_object:

		// skip whitespaces
		buff = skip_whitespace(buff, end);
		if(buff >= end)
		{
			ks->state = KSTATE_KEY_IN_OBJECT;
			return KS_JSON_MORE_DATA;
		}

		// object/array could be empty
		if(*buff == ks->array)
			goto skip_empty_object;

		if(ks->array == ']')
			goto continue_val_in_object;

		// must get string (key)
		if(*buff != '"')
		{
			ks->error = KS_JSON_EXPECTED_KEY;
			goto finished;
		}
		buff++;

		// 'key' string
		ks->ptr = ks->str;
continue_key_string:
		buff = get_string(ks, buff, end);
		if(!buff)
		{
			ks->error = KS_JSON_TOO_LONG_STRING;
			goto finished;
		}
		if(buff >= end)
		{
			ks->state = KSTATE_KEY_STRING;
			return KS_JSON_MORE_DATA;
		}

		*ks->ptr = 0; // string terminator
		buff++; // skip string terminator

printf("got key: '%s'\n", ks->str);

		// skip whitespaces
continue_key_separator:
		buff = skip_whitespace(buff, end);
		if(buff >= end)
		{
			ks->state = KSTATE_KEY_SEPARATOR;
			return KS_JSON_MORE_DATA;
		}

		// check for key separator
		if(*buff != ':')
		{
			ks->error = KS_JSON_EXPECTED_KEY_SEPARATOR;
			goto finished;
		}
		buff++;

continue_val_in_object:

		// skip whitespaces
		buff = skip_whitespace(buff, end);
		if(buff >= end)
		{
			ks->state = KSTATE_VAL_IN_OBJECT;
			return KS_JSON_MORE_DATA;
		}

		// prepare to read
		ks->ptr = ks->str;
		// check type
		if(*buff == '"')
		{
			buff++;
continue_val_string:
			// string
			buff = get_string(ks, buff, end);
			if(!buff)
			{
				ks->error = KS_JSON_TOO_LONG_STRING;
				goto finished;
			}
			if(buff >= end)
			{
				ks->state = KSTATE_VAL_STRING;
				return KS_JSON_MORE_DATA;
			}
			buff++; // skip string terminator
			ks->val_type = JTYPE_STRING;
		} else
		if(*buff == '{')
		{
			// prepare object
			buff++;
			ks->array = '}';
			ks->depth++;
			if(ks->depth == KS_JSON_MAX_DEPTH)
			{
				ks->error = KS_JSON_TOO_DEEP;
				goto finished;
			}
			ks->dbit[ks->depth >> 5] &= ~(1 << (ks->depth & 31));
printf("object; depth %u; bits 0x%08X\n\n", ks->depth, ks->dbit[ks->depth >> 5]);
			goto continue_key_in_object;
		} else
		if(*buff == '[')
		{
			// prepare array
			buff++;
			ks->array = ']';
			ks->depth++;
			if(ks->depth == KS_JSON_MAX_DEPTH)
			{
				ks->error = KS_JSON_TOO_DEEP;
				goto finished;
			}
			ks->dbit[ks->depth >> 5] |= (1 << (ks->depth & 31));
printf("array; depth %u; bits 0x%08X\n\n", ks->depth, ks->dbit[ks->depth >> 5]);
			goto continue_key_in_object;
		} else
		{
			// other
continue_val_other:
			while(buff < end)
			{
				register uint8_t tmp = *buff;
				if(tmp == ',' || tmp == '}' || tmp == ']' || tmp == ' ' || tmp == '\t' || tmp == '\n' || tmp == '\r')
					break;
				*ks->ptr++ = tmp;
				if(ks->ptr == ks->str + KS_JSON_MAX_STRING_LENGTH)
				{
					ks->error = KS_JSON_TOO_LONG_STRING;
					goto finished;
				}
				buff++;
			}
			if(buff >= end)
			{
				ks->state = KSTATE_VAL_OTHER;
				return KS_JSON_MORE_DATA;
			}
			// TODO: determine type
			ks->val_type = JTYPE_OTHER;
		}
		*ks->ptr = 0; // string terminator

		// skip whitespaces
continue_val_end:
		buff = skip_whitespace(buff, end);
		if(buff >= end)
		{
			ks->state = KSTATE_VAL_END;
			return KS_JSON_MORE_DATA;
		}

printf("got val: '%s'\n\n", ks->str);

skip_empty_object:
		while(*buff == ks->array)
		{
			if(!ks->depth)
			{
				// finished
printf("FINISHED\n");
				ks->error = KS_JSON_OK;
				goto finished;
			}
printf("drop depth %d\n\n", ks->depth);
			ks->depth--;
			ks->array = ks->dbit[ks->depth >> 5] & (1 << (ks->depth & 31)) ? ']' : '}';
			buff++;
			// skip whitespaces
continue_termination:
			buff = skip_whitespace(buff, end);
			if(buff >= end)
			{
				ks->state = KSTATE_TERMINATION;
				return KS_JSON_MORE_DATA;
			}
		}

		// check for next value
		if(*buff == ',')
		{
			buff++;
			goto continue_key_in_object;
		}

		ks->error = KS_JSON_SYNTAX;
		goto finished;
	}

finished:
	ks->state = KSTATE_FINISHED;
	return ks->error;
}

void ks_json_reset(kgstruct_json_t *ks)
{
	ks->state = KSTATE_START_OBJECT;
	ks->depth = 0;
	ks->array = '}';
}

