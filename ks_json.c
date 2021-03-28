#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "kgstruct.h"
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

// TODO: move elsewhere
static const uint8_t kgstruct_type_size[] =
{
	[KS_TYPEDEF_U8] = sizeof(uint8_t),
	[KS_TYPEDEF_S8] = sizeof(int8_t),
	[KS_TYPEDEF_U16] = sizeof(uint16_t),
	[KS_TYPEDEF_S16] = sizeof(int16_t),
	[KS_TYPEDEF_U32] = sizeof(uint32_t),
	[KS_TYPEDEF_S32] = sizeof(int32_t),
#ifdef KGSTRUCT_ENABLE_US64
	[KS_TYPEDEF_U64] = sizeof(uint64_t),
	[KS_TYPEDEF_S64] = sizeof(int64_t),
#endif
#ifdef KGSTRUCT_ENABLE_FLOAT
	[KS_TYPEDEF_FLOAT] = sizeof(float),
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
	[KS_TYPEDEF_DOUBLE] = sizeof(double),
#endif
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

#ifdef KGSTRUCT_ENABLE_US64
static uint8_t *get_unsigned(uint8_t *str, uint64_t *dst)
{
	uint64_t val = 0;
#else
static uint8_t *get_unsigned(uint8_t *str, uint32_t *dst)
{
	uint32_t val = 0;
#endif
	while(1)
	{
		register uint8_t tmp = *str;
		if(!tmp)
			break;
		if(tmp == '.')
			break;
		if(tmp < '0' || tmp > '9')
			return NULL;
		val *= 10;
		val += tmp - '0';
		str++;
	}

	*dst = val;

	return str;
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

		// find this key in the template
		ks->element = NULL;
		if(!ks->depth_ignored)
		{
			const kgstruct_template_t *elm = ks->template;
			while(elm->info)
			{
				if(elm->magic == ks->magic && elm->key && !strcmp(elm->key, ks->str))
				{
printf("** found match for key at %u **\n", (uint32_t)(elm - ks->template));
					ks->element = elm;
					break;
				}
				elm++;
			}
		}

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

		// advance array index
		if(!ks->depth_ignored && ks->array_max)
			ks->array_idx++;

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
			// check for type match
			if(ks->element && (ks->element->info->base.type & (KS_TYPEMASK_TYPE | KS_TYPEFLAG_IS_ARRAY)) == KS_TYPEDEF_STRUCT)
				// change magic and continue
				ks->magic = ks->element->offset;
			else
				// ignore this branch
				ks->depth_ignored++;
printf("object; depth %u; bits 0x%08X; ignored %u; magic %u\n\n", ks->depth, ks->dbit[ks->depth >> 5], ks->depth_ignored, ks->magic);
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
			// check for type match
			if(ks->element && ks->element->info->base.type & KS_TYPEFLAG_IS_ARRAY)
			{
				if(ks->array_max)
				{
					// array in array is not supported
					ks->error = KS_JSON_TOO_DEEP;
					goto finished;
				}
				ks->array_idx = 0xFFFF;
				ks->array_max = (ks->element->info->base.type & KS_TYPEMASK_ARRAY) >> KS_TYPESHIFT_ARRAY;
				if((ks->element->info->base.type & KS_TYPEMASK_TYPE) == KS_TYPEDEF_STRUCT)
					// get structure size
					ks->array_es = ks->element->info->object.extra[0];
				else
					// get size by type
					ks->array_es = kgstruct_type_size[ks->element->info->base.type & KS_TYPEMASK_TYPE];
				ks->array_el = ks->element;
				ks->element = NULL;
printf("** array; elm size is %u\n", ks->array_es);
			} else
				// ignore this branch
				ks->depth_ignored++;
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
			// type
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

		// check for skip
		if(ks->depth_ignored)
			goto skip_empty_object;

		// check for array
		if(ks->array_max && ks->array_idx < ks->array_max)
			// array can fit this index
			ks->element = ks->array_el;

		// process this value
		if(ks->element)
		{
			uint32_t type = ks->element->info->base.type;
			if((type & KS_TYPEMASK_TYPE) == KS_TYPEDEF_STRING)
			{
				// TODO: check type?
				// check string length
				type = ks->ptr - ks->str;
				if(type > ks->element->info->string.extra[0])
					 ks->str[ks->element->info->string.extra[0]] = 0;
				// copy the string
				strcpy(ks->data + ks->element->offset, ks->str);
			} else
			if(ks->val_type == JTYPE_OTHER)
			{
				int neg_bad; // -1 = bad; 0 = positive; 1 = negative
				uint8_t *ptr;
				kgstruct_number_t val;
				kgstruct_number_t *dst = ks->data + ks->element->offset;
				// array index check
				if(ks->array_max)
					dst = (void*)dst + ks->array_idx * ks->array_es;
				// boolean check
				if(!strcmp(ks->str, "true"))
				{
					val.uread = 1;
					neg_bad = 0;
				} else
				if(!strcmp(ks->str, "false"))
				{
					val.uread = 0;
					neg_bad = 0;
				} else
				{
					// normal number
					ptr = ks->str;

					if(*ptr == '-')
					{
						neg_bad = 1;
						ptr++;
					} else
						neg_bad = 0;

					ptr = get_unsigned(ptr, &val.uread);
					if(!ptr)
						neg_bad = -1;
				}
				// assign
				if(neg_bad >= 0)
				{
#ifdef KGSTRUCT_ENABLE_MINAX
					int idx = 0;
#endif
					switch(type & KS_TYPEMASK_TYPE)
					{
						// 8 bits
						case KS_TYPEDEF_U8:
							if(neg_bad)
								break;
#ifdef KGSTRUCT_ENABLE_MINAX
							if(type & KS_TYPEFLAG_HAS_MIN)
							{
								if(val.uread < ks->element->info->u8.extra[0])
									break;
								idx++;
							}
							if(type & KS_TYPEFLAG_HAS_MAX)
							{
								if(val.uread > ks->element->info->u8.extra[idx])
									break;
							} else
#endif
							if(val.uread > 0xFF)
								break;
							dst->u8 = val.u8;
						break;
						case KS_TYPEDEF_S8:
							if(neg_bad)
							{
								if(val.uread > KGSTRUCT_NUMBER_SMAX)
									break;
								val.sread = -val.sread;
							}
#ifdef KGSTRUCT_ENABLE_MINAX
							if(type & KS_TYPEFLAG_HAS_MIN)
							{
								if(val.sread < ks->element->info->s8.extra[0])
									break;
								idx++;
							} else
#endif
							if(val.sread < -0x80)
								break;
#ifdef KGSTRUCT_ENABLE_MINAX
							if(type & KS_TYPEFLAG_HAS_MAX)
							{
								if(val.sread > ks->element->info->s8.extra[idx])
									break;
							} else
#endif
							if(val.sread > 0x7F)
								break;
							dst->s8 = val.s8;
						break;
						// 16 bits
						case KS_TYPEDEF_U16:
							if(neg_bad)
								break;
#ifdef KGSTRUCT_ENABLE_MINAX
							if(type & KS_TYPEFLAG_HAS_MIN)
							{
								if(val.uread < ks->element->info->u16.extra[0])
									break;
								idx++;
							}
							if(type & KS_TYPEFLAG_HAS_MAX)
							{
								if(val.uread > ks->element->info->u16.extra[idx])
									break;
							} else
#endif
							if(val.uread > 0xFFFF)
								break;
							dst->u16 = val.u16;
						break;
						case KS_TYPEDEF_S16:
							if(neg_bad)
							{
								if(val.uread > KGSTRUCT_NUMBER_SMAX)
									break;
								val.sread = -val.sread;
							}
#ifdef KGSTRUCT_ENABLE_MINAX
							if(type & KS_TYPEFLAG_HAS_MIN)
							{
								if(val.sread < ks->element->info->s16.extra[0])
									break;
								idx++;
							} else
#endif
							if(val.sread < -0x8000)
								break;
#ifdef KGSTRUCT_ENABLE_MINAX
							if(type & KS_TYPEFLAG_HAS_MAX)
							{
								if(val.sread > ks->element->info->s16.extra[idx])
									break;
							} else
#endif
							if(val.sread > 0x7FFF)
								break;
							dst->s16 = val.s16;
						break;
						// 32 bits
						case KS_TYPEDEF_U32:
							if(neg_bad)
								break;
#ifdef KGSTRUCT_ENABLE_MINAX
							if(type & KS_TYPEFLAG_HAS_MIN)
							{
								if(val.uread < ks->element->info->u32.extra[0])
									break;
								idx++;
							}
							if(type & KS_TYPEFLAG_HAS_MAX)
							{
								if(val.uread > ks->element->info->u32.extra[idx])
									break;
							} else
#endif
							if(val.uread > 0xFFFFFFFF)
								break;
							dst->u32 = val.u32;
						break;
						case KS_TYPEDEF_S32:
							if(neg_bad)
							{
								if(val.uread > KGSTRUCT_NUMBER_SMAX)
									break;
								val.sread = -val.sread;
							}
#ifdef KGSTRUCT_ENABLE_MINAX
							if(type & KS_TYPEFLAG_HAS_MIN)
							{
								if(val.sread < ks->element->info->s32.extra[0])
									break;
								idx++;
							} else
#endif
							if(val.sread < -0x80000000L)
								break;
#ifdef KGSTRUCT_ENABLE_MINAX
							if(type & KS_TYPEFLAG_HAS_MAX)
							{
								if(val.sread > ks->element->info->s32.extra[idx])
									break;
							} else
#endif
							if(val.sread > 0x7FFFFFFF)
								break;
							dst->s32 = val.s32;
						break;
						// 64 bits
#ifdef KGSTRUCT_ENABLE_US64
						case KS_TYPEDEF_U64:
							if(neg_bad)
								break;
#ifdef KGSTRUCT_ENABLE_MINAX
							if(type & KS_TYPEFLAG_HAS_MIN)
							{
								if(val.uread < ks->element->info->u64.extra[0])
									break;
								idx++;
							}
							if(type & KS_TYPEFLAG_HAS_MAX)
							{
								if(val.uread > ks->element->info->u64.extra[idx])
									break;
							}
#endif
							dst->u64 = val.u64;
						break;
						case KS_TYPEDEF_S64:
							if(neg_bad)
							{
								if(val.uread > KGSTRUCT_NUMBER_SMAX)
									break;
								val.sread = -val.sread;
							}
#ifdef KGSTRUCT_ENABLE_MINAX
							if(type & KS_TYPEFLAG_HAS_MIN)
							{
								if(val.sread < ks->element->info->s64.extra[0])
									break;
								idx++;
							}
							if(type & KS_TYPEFLAG_HAS_MAX)
							{
								if(val.sread > ks->element->info->s64.extra[idx])
									break;
							}
#endif
							dst->s64 = val.s64;
						break;
#endif
						// float
#ifdef KGSTRUCT_ENABLE_FLOAT
						case KS_TYPEDEF_FLOAT:
							if(neg_bad)
								val.sread = -val.sread;
#ifdef KGSTRUCT_ENABLE_MINAX
							if(type & KS_TYPEFLAG_HAS_MIN)
							{
								if(val.sread < ks->element->info->f32.extra[0])
									break;
								idx++;
							}
							if(type & KS_TYPEFLAG_HAS_MAX)
							{
								if(val.sread > ks->element->info->f32.extra[idx])
									break;
							}
#endif
							dst->f32 = val.sread;
							// decimal part
							if(*ptr == '.')
							{
								float dec = 0;
								while(1)
								{
									register uint8_t tmp;
									ptr++;
									tmp = *ptr;
									if(tmp < '0' || tmp > '9')
										break;
								}
								while(1)
								{
									register uint8_t tmp;
									ptr--;
									tmp = *ptr;
									if(!tmp)
										break;
									if(tmp < '0' || tmp > '9')
										break;
									dec += tmp - '0';
									dec /= 10;
								}
								if(neg_bad)
									dec = -dec;
								dst->f32 += dec;
							}
						break;
#endif
						// double
#ifdef KGSTRUCT_ENABLE_DOUBLE
						case KS_TYPEDEF_DOUBLE:
							if(neg_bad)
								val.sread = -val.sread;
#ifdef KGSTRUCT_ENABLE_MINAX
							if(type & KS_TYPEFLAG_HAS_MIN)
							{
								if(val.sread < ks->element->info->f64.extra[0])
									break;
								idx++;
							}
							if(type & KS_TYPEFLAG_HAS_MAX)
							{
								if(val.sread > ks->element->info->f64.extra[idx])
									break;
							}
#endif
							dst->f64 = val.sread;
							// decimal part
							if(*ptr == '.')
							{
								double dec = 0;
								while(1)
								{
									register uint8_t tmp;
									ptr++;
									tmp = *ptr;
									if(tmp < '0' || tmp > '9')
										break;
								}
								while(1)
								{
									register uint8_t tmp;
									ptr--;
									tmp = *ptr;
									if(!tmp)
										break;
									if(tmp < '0' || tmp > '9')
										break;
									dec += tmp - '0';
									dec /= 10;
								}
								if(neg_bad)
									dec = -dec;
								dst->f32 += dec;
							}
						break;
#endif
					}
				}
			}
			ks->element = NULL;
		}

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
printf("drop depth %d ign %d\n\n", ks->depth, ks->depth_ignored);
			if(!ks->depth_ignored)
			{
				if(!ks->array_max)
				{
					// find old magic
					const kgstruct_template_t *elm = ks->template;
					while(elm->info)
					{
						if(!elm->key && elm->magic == ks->magic)
						{
							ks->magic = elm->offset;
printf("** found old magic %u **\n", ks->magic);
							break;
						}
						elm++;
					}
				} else
				{
					// leave the array
					ks->array_max = 0;
printf("** leaving array **\n");
				}
			} else
				// drop ignored depth
				ks->depth_ignored--;
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

void ks_json_init(kgstruct_json_t *ks, void *ptr, const kgstruct_template_t *template)
{
	ks->data = ptr;
	ks->template = template;
	ks->state = KSTATE_START_OBJECT;
	ks->depth = 0;
	ks->depth_ignored = 0;
	ks->escaped = 0;
	ks->array = '}';
	ks->magic = 0;
	ks->array_max = 0;
}

