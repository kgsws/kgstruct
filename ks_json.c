#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#ifdef KGSTRUCT_EXTERNAL_CONFIG
#include KGSTRUCT_EXTERNAL_CONFIG
#endif
#include "kgstruct.h"
#include "ks_json.h"

//#define KS_JSON_DEBUG

#if KS_JSON_MAX_STRING_LENGTH < 16
#error [kgJSON] max string length is too short!
#endif

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

static const ks_template_t dummy_object[1] =
{
	{}
};

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
// export chain
#ifdef KS_JSON_EXPORTER
static uint8_t *export_nl_indent_key(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end);
static uint8_t *export_object_entry(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end);
static uint8_t *export_next_key(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end);
static uint8_t *export_array_next(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end);
static uint8_t *export_array_nl_close(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end);
static uint8_t *export_value_start(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end);

static uint8_t *export_object_close(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	*buff++ = '}';

	if(ks->depth)
	{
		ks->depth--;

		if(ks->recursion[ks->depth].step)
		{
			ks->recursion[ks->depth].limit--;

			if(ks->recursion[ks->depth].limit)
			{
				ks->recursion[ks->depth].offset += ks->recursion[ks->depth].step;
				ks->export_step = export_array_next;
				return buff;
			}

			ks->depth--;
			ks->export_step = export_array_nl_close;
			return buff;
		} else
			ks->export_step = export_next_key;
	} else
		ks->export_step = NULL;

	return buff;
}

static uint8_t *export_next_key(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
#ifdef KS_JSON_ENABLE_FILLINFO
	ks->recursion[ks->depth].fill_idx++;
#endif
	ks->recursion[ks->depth].tmpl++;
	if(!ks->recursion[ks->depth].tmpl->key)
	{
		if(ks->readable)
		{
			*buff++ = '\n';
			ks->val_type = ks->depth;
		}
		ks->export_step = export_object_close;
		return buff;
	}
	*buff++ = ',';
	ks->export_step = export_nl_indent_key;
	return buff;
}

static uint8_t *export_array_next_nl(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	if(ks->readable)
	{
		*buff++ = '\n';
		ks->val_type = ks->depth + 1;
	}
	ks->export_step = export_value_start;
	return buff;
}

static uint8_t *export_array_next(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
#ifdef KS_JSON_ENABLE_FILLINFO
	if(ks->recursion[ks->depth].tmpl->info->base.type == KS_TYPEDEF_STRUCT)
		ks->recursion[ks->depth].fill_offset += ks->recursion[ks->depth].tmpl->info->object.basetemp->fill_size;
#endif
	*buff++ = ',';
	ks->export_step = export_array_next_nl;
	return buff;
}

static uint8_t *export_array_close(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	*buff++ = ']';
	ks->export_step = export_next_key;
	return buff;
}

static uint8_t *export_array_nl_close(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	if(ks->readable)
	{
		*buff++ = '\n';
		ks->val_type = ks->depth + 1;
	}
	ks->export_step = export_array_close;
	return buff;
}

static uint8_t *export_value_stop(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	if(ks->state)
		*buff++ = '"';

	if(ks->recursion[ks->depth].step)
	{
		ks->recursion[ks->depth].limit--;

		if(ks->recursion[ks->depth].limit)
		{
			ks->recursion[ks->depth].offset += ks->recursion[ks->depth].step;
			ks->export_step = export_array_next;
			return buff;
		}

		ks->depth--;
		ks->export_step = export_array_nl_close;
		return buff;
	}

	ks->export_step = export_next_key;
	return buff;
}

static uint8_t *export_value(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	while(*ks->ptr)
	{
		uint8_t tmp = *ks->ptr;
		if(ks->escaped)
		{
			ks->escaped = 0;
			ks->ptr++;
			switch(tmp)
			{
				case '\n':
					tmp = 'n';
				break;
				case '\r':
					tmp = 'r';
				break;
				case '\t':
					tmp = 't';
				break;
			}
		} else
		{
			if(tmp == '\n' || tmp == '\r' || tmp == '\t' || tmp == '"' || tmp == '\\')
			{
				tmp = '\\';
				ks->escaped = 1;
			} else
				ks->ptr++;
		}
		*buff++ = tmp;
		if(buff == end)
			return buff;
	}
	ks->export_step = export_value_stop;
	return buff;
}

static uint8_t *export_object_entry_nl(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	if(ks->readable)
	{
		*buff++ = '\n';
		ks->val_type = ks->depth;
	}
	ks->export_step = export_object_entry;
	return buff;
}

static uint8_t *export_value_start(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	kgstruct_number_t *value;

	// init value
	ks->state = 0;
	ks->ptr = ks->str;

	value = ks->data + ks->recursion[ks->depth].offset + ks->recursion[ks->depth].tmpl->offset;

	if(	ks->recursion[ks->depth].tmpl->info->base.flags & KS_TYPEFLAG_IS_BOOL &&
		ks->recursion[ks->depth].tmpl->info->base.type < KS_TYPEDEF_STRING
	){
		kgstruct_number_t val;
		val.uval = 0;
		switch(ks->recursion[ks->depth].tmpl->info->base.type)
		{
			case KS_TYPEDEF_U8:
			case KS_TYPEDEF_S8:
				val.u8 = value->u8;
			break;
			case KS_TYPEDEF_U16:
			case KS_TYPEDEF_S16:
				val.u16 = value->u16;
			break;
			case KS_TYPEDEF_U32:
			case KS_TYPEDEF_S32:
#ifdef KGSTRUCT_ENABLE_FLOAT
			case KS_TYPEDEF_FLOAT:
#endif
				val.u32 = value->u32;
			break;
#ifdef KGSTRUCT_ENABLE_DOUBLE
			case KS_TYPEDEF_DOUBLE:
				val.f64 = value->f64;
			break;
#endif
#ifdef KGSTRUCT_ENABLE_US64
			case KS_TYPEDEF_U64:
			case KS_TYPEDEF_S64:
				val.u64 = value->u64;
			break;
#endif
		}
		if(val.uval)
			strcpy(ks->str, "true");
		else
			strcpy(ks->str, "false");
	} else
	{
		switch(ks->recursion[ks->depth].tmpl->info->base.type)
		{
			case KS_TYPEDEF_U8:
				sprintf(ks->str, "%u", value->u8);
			break;
			case KS_TYPEDEF_S8:
				sprintf(ks->str, "%d", value->s8);
			break;
			case KS_TYPEDEF_U16:
				sprintf(ks->str, "%u", value->u16);
			break;
			case KS_TYPEDEF_S16:
				sprintf(ks->str, "%d", value->s16);
			break;
			case KS_TYPEDEF_U32:
				sprintf(ks->str, "%u", value->u32);
			break;
			case KS_TYPEDEF_S32:
				sprintf(ks->str, "%d", value->s32);
			break;
#ifdef KGSTRUCT_ENABLE_US64
			case KS_TYPEDEF_U64:
#ifdef WINDOWS
				sprintf(ks->str, "%I64u", value->u64);
#else
				sprintf(ks->str, "%lu", value->u64);
#endif
			break;
			case KS_TYPEDEF_S64:
#ifdef WINDOWS
				sprintf(ks->str, "%I64d", value->s64);
#else
				sprintf(ks->str, "%ld", value->s64);
#endif
			break;
#endif
#ifdef KGSTRUCT_ENABLE_FLOAT
			case KS_TYPEDEF_FLOAT:
				sprintf(ks->str, "%f", value->f32);
			break;
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
			case KS_TYPEDEF_DOUBLE:
				sprintf(ks->str, "%lf", value->f64);
			break;
#endif
			case KS_TYPEDEF_STRING:
				ks->ptr = &value->u8;
				ks->state = 1;
			break;
			case KS_TYPEDEF_STRUCT:
#ifdef KGSTRUCT_ENABLE_FLAGS
			case KS_TYPEDEF_FLAGS:
#endif
				if(ks->recursion[ks->depth].step)
					ks->export_step = export_object_entry;
				else
					ks->export_step = export_object_entry_nl;
				ks->depth++;
				ks->recursion[ks->depth].tmpl = ks->recursion[ks->depth-1].tmpl->info->object.basetemp->tmpl;
				ks->recursion[ks->depth].offset = ks->recursion[ks->depth-1].offset + ks->recursion[ks->depth-1].tmpl->offset;
				ks->recursion[ks->depth].step = 0;
#ifdef KS_JSON_ENABLE_FILLINFO
				ks->recursion[ks->depth].fill_offset = ks->recursion[ks->depth-1].fill_offset + ks->recursion[ks->depth-1].tmpl->fill_offs;
				ks->recursion[ks->depth].fill_step = 0;
				ks->recursion[ks->depth].fill_idx = 0;
#endif
			return buff;
#ifdef KGSTRUCT_ENABLE_CUSTOM_TYPE
			case KS_TYPEDEF_CUSTOM:
				ks->state = ks->recursion[ks->depth].tmpl->info->custom.exprt(value, ks->ptr);
			break;
#endif
#ifdef KGSTRUCT_ENABLE_FLAGS
			case KS_TYPEDEF_FLAG8:
				if(value->u8 & ks->recursion[ks->depth].tmpl->flag_bits)
					strcpy(ks->str, "true");
				else
					strcpy(ks->str, "false");
			break;
			case KS_TYPEDEF_FLAG16:
				if(value->u16 & ks->recursion[ks->depth].tmpl->flag_bits)
					strcpy(ks->str, "true");
				else
					strcpy(ks->str, "false");
			break;
			case KS_TYPEDEF_FLAG32:
				if(value->u32 & ks->recursion[ks->depth].tmpl->flag_bits)
					strcpy(ks->str, "true");
				else
					strcpy(ks->str, "false");
			break;
#ifdef KGSTRUCT_ENABLE_US64
			case KS_TYPEDEF_FLAG64:
				if(value->u64 & ks->recursion[ks->depth].tmpl->flag_bits)
					strcpy(ks->str, "true");
				else
					strcpy(ks->str, "false");
			break;
#endif
#endif
			default:
				ks->ptr = "ERROR";
				ks->state = 1;
			break;
		}
	}

	// export start
	if(ks->state)
		*buff++ = '"';
	ks->export_step = export_value;

	return buff;
}

static uint8_t *export_array_start_nl(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	if(ks->readable)
	{
		*buff++ = '\n';
		ks->val_type = ks->depth + 1;
	}
	ks->export_step = export_value_start;
	return buff;
}

static uint8_t *export_array_start(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	*buff++ = '[';
	if(!ks->recursion[ks->depth].limit)
	{
		ks->depth--;
		ks->export_step = export_array_nl_close;
	} else
		ks->export_step = export_array_start_nl;
	return buff;
}

static uint8_t *export_key_separator_or_array(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	if(ks->recursion[ks->depth].tmpl->info->base.array)
	{
		const ks_template_t *element = ks->recursion[ks->depth].tmpl;
		ks->depth++;
		ks->recursion[ks->depth].tmpl = element;
		ks->recursion[ks->depth].offset = ks->recursion[ks->depth-1].offset;
		ks->recursion[ks->depth].limit = ks->recursion[ks->depth].tmpl->info->base.array;
#ifdef KS_JSON_ENABLE_FILLINFO
		ks->recursion[ks->depth].fill_offset = ks->recursion[ks->depth-1].fill_offset;
		ks->recursion[ks->depth].fill_step = 0;
		ks->recursion[ks->depth].fill_idx = ks->recursion[ks->depth-1].fill_idx;
		if(ks->fillinfo && element->info->base.flags & KS_TYPEFLAG_EMPTY_ARRAY)
		{
			uint32_t depth = ks->depth - 1;
			KGSTRUCT_FILLINFO_TYPE *count = ks->fillinfo + ks->recursion[depth].fill_offset + ks->recursion[depth].fill_idx * sizeof(KGSTRUCT_FILLINFO_TYPE);
			if(*count < ks->recursion[ks->depth].limit)
				ks->recursion[ks->depth].limit = *count;
		}
#endif
		if(element->info->base.type < KS_TYPE_LAST_NUMERIC)
			ks->recursion[ks->depth].step = kgstruct_type_size[element->info->base.type];
		else
			ks->recursion[ks->depth].step = element->info->base.size;

		if(ks->readable)
		{
			*buff++ = '\n';
			ks->val_type = ks->depth;
		}
		ks->export_step = export_array_start;

		return buff;
	}

	if(ks->readable && ks->recursion[ks->depth].tmpl->info->base.type != KS_TYPEDEF_STRUCT)
		*buff++ = ' ';
	ks->export_step = export_value_start;

	return buff;
}

static uint8_t *export_key_separator(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	*buff++ = ':';
	ks->export_step = export_key_separator_or_array;
	return buff;
}

static uint8_t *export_key_stop(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	*buff++ = '"';
	ks->export_step = export_key_separator;
	return buff;
}

static uint8_t *export_key(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	while(*ks->ptr)
	{
		*buff++ = *ks->ptr++;
		if(buff == end)
			return buff;
	}
	ks->export_step = export_key_stop;
	return buff;
}

static uint8_t *export_key_start(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	*buff++ = '"';
	ks->export_step = export_key;
	ks->ptr = (uint8_t*)ks->recursion[ks->depth].tmpl->key;
	return buff;
}

static uint8_t *export_nl_indent_key(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	if(ks->readable)
	{
		*buff++ = '\n';
		ks->val_type = ks->depth + 1;
	}
	ks->export_step = export_key_start;
	return buff;
}

static uint8_t *export_object_entry(kgstruct_json_t *ks, uint8_t *buff, uint8_t *end)
{
	*buff++ = '{';
	ks->export_step = export_nl_indent_key;
	return buff;
}

#endif

//
// funcs

static const uint8_t *skip_whitespace(kgstruct_json_t *ks, const uint8_t *ptr, const uint8_t *end)
{
	while(ptr < end)
	{
		register uint8_t tmp = *ptr;
		if(tmp != ' ' && tmp != '\t' && tmp != '\n' && tmp != '\r')
			break;
#ifdef KS_JSON_LINE_COUNTER
		if(tmp == '\n')
			ks->line++;
#endif
		ptr++;
	}
	return ptr;
}

static const uint8_t *get_string(kgstruct_json_t *ks, const uint8_t *ptr, const uint8_t *end)
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
		{
			ks->escaped = 0;
			switch(tmp)
			{
				case 'r':
					tmp = '\r';
				break;
				case 'n':
					tmp = '\n';
				break;
				case 't':
					tmp = '\t';
				break;
				case 'b':
					tmp = '\b';
				break;
				case 'f':
					tmp = '\f';
				break;
			}
		}
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

#ifdef KS_JSON_PARSER

#ifdef KS_JSON_ENABLE_FILLINFO
void update_fillinfo(kgstruct_json_t *ks, uint32_t depth, uint32_t what)
{
	if(!ks->fillinfo)
		return;
	if(ks->fill_idx < 0)
		return;
	if(ks->recursion[depth].fill_offset == 0xFFFFFFFF)
		return;
#ifdef KS_JSON_DEBUG
	printf("FILLINFO: base offs %u; offs %u; what %u\n", ks->recursion[depth].fill_offset, ks->fill_idx, what);
#endif
	KGSTRUCT_FILLINFO_TYPE *counter = ks->fillinfo + ks->recursion[depth].fill_offset + ks->fill_idx * sizeof(KGSTRUCT_FILLINFO_TYPE);

	if(what)
		*counter = *counter + 1;
	else
		*counter = 0;
}
#endif

int ks_json_parse(kgstruct_json_t *ks, const uint8_t *buff, uint32_t length)
{
	const uint8_t *end = buff + length;
#ifdef KS_JSON_ENABLE_FILLINFO
	uint_fast8_t was_parsed;
#endif

	if(buff == end)
		return KS_JSON_MORE_DATA;

	switch(ks->state)
	{
		case KSTATE_FINISHED:
			return ks->error;
		case KSTATE_KEY_IN_OBJECT:
			goto continue_key_in_object;
		case KSTATE_KEY_STRING:
			goto continue_key_string;
		case KSTATE_KEY_SEPARATOR:
			goto continue_key_separator;
		case KSTATE_VAL_IN_OBJECT:
			goto continue_val_in_object;
		case KSTATE_VAL_STRING:
			goto continue_val_string;
		case KSTATE_VAL_OTHER:
			goto continue_val_other;
		case KSTATE_VAL_END:
			goto continue_val_end;
		case KSTATE_TERMINATION:
			goto continue_termination;
		case KSTATE_START_OBJECT:
			break;
		default:
			return KS_JSON_BAD_STATE;
	}

	while(1)
	{
		// skip whitespaces
		buff = skip_whitespace(ks, buff, end);
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
		buff = skip_whitespace(ks, buff, end);
		if(buff >= end)
		{
			ks->state = KSTATE_KEY_IN_OBJECT;
			return KS_JSON_MORE_DATA;
		}

		// object/array could be empty
		if(*buff == ks->array)
			goto skip_empty_object;

		// are we in array?
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
#ifdef KS_JSON_DEBUG
		printf("got key: '%s'\n", ks->str);
#endif
		// find this key in the template
		ks->element = NULL;
#ifdef KS_JSON_ENABLE_FILLINFO
		ks->fill_idx = -1;
#endif
		if(ks->recursion[ks->depth].tmpl)
		{
			const ks_template_t *elm = ks->recursion[ks->depth].tmpl;
			while(elm->info)
			{
				if(!strcmp((void*)elm->key, (void*)ks->str))
				{
#ifdef KS_JSON_DEBUG
					printf("** found match for key at %u **\n", (uint32_t)(elm - ks->recursion[ks->depth].tmpl));
#endif
					ks->element = elm;
#ifdef KS_JSON_ENABLE_FILLINFO
					ks->fill_idx = (uint32_t)(elm - ks->recursion[ks->depth].tmpl);
					update_fillinfo(ks, ks->depth, 0);
#endif
					break;
				}
				elm++;
			}
		}

		// skip whitespaces
continue_key_separator:
		buff = skip_whitespace(ks, buff, end);
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
		buff = skip_whitespace(ks, buff, end);
		if(buff >= end)
		{
			ks->state = KSTATE_VAL_IN_OBJECT;
			return KS_JSON_MORE_DATA;
		}

		// advance array index
#ifdef KS_JSON_DEBUG
		printf("base offset %u step %u limit %u; etype %d\n", ks->recursion[ks->depth].offset, ks->recursion[ks->depth].step, ks->recursion[ks->depth].limit, ks->element ? ks->element->info->base.type : -1);
#endif

		ks->recursion[ks->depth].offset += ks->recursion[ks->depth].step;
		if(ks->recursion[ks->depth].offset >= ks->recursion[ks->depth].limit)
		{
			ks->element = NULL;
			ks->recursion[ks->depth].tmpl = NULL;
#ifdef KS_JSON_ENABLE_FILLINFO
			ks->fill_idx = -1;
#endif
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
			uint32_t offset = ks->recursion[ks->depth].offset;
			// prepare object
			buff++;
			ks->array = '}';
#ifdef KS_JSON_ENABLE_FILLINFO
			update_fillinfo(ks, ks->depth, 1);
#endif
			ks->depth++;
			if(ks->depth >= KS_JSON_MAX_DEPTH)
			{
				ks->error = KS_JSON_TOO_DEEP;
				goto finished;
			}
			// check for type match
			if(	ks->element && (
				ks->element->info->base.type == KS_TYPEDEF_STRUCT
#ifdef KGSTRUCT_ENABLE_FLAGS
				|| ks->element->info->base.type == KS_TYPEDEF_FLAGS
#endif
			)) {
				ks->recursion[ks->depth].tmpl = ks->element->info->object.basetemp->tmpl;
				ks->recursion[ks->depth].offset = offset + ks->element->offset;
#ifdef KS_JSON_DEBUG
				printf("object; depth %u; offset %u\n\n", ks->depth, ks->recursion[ks->depth].offset);
#endif
#ifdef KS_JSON_ENABLE_FILLINFO
#ifdef KGSTRUCT_ENABLE_FLAGS
				if(ks->element->info->base.type == KS_TYPEDEF_FLAGS)
					ks->recursion[ks->depth].fill_offset = 0xFFFFFFFF;
				else
#endif
					ks->recursion[ks->depth].fill_offset = ks->recursion[ks->depth-1].fill_offset + ks->element->fill_offs + ks->recursion[ks->depth-1].fill_step;
				ks->recursion[ks->depth].fill_step = 0;
#endif
			} else
			{
				ks->recursion[ks->depth].tmpl = dummy_object;
#ifdef KS_JSON_DEBUG
				printf("object; depth %u; UNKNOWN\n\n", ks->depth);
#endif
			}
			ks->recursion[ks->depth].step = 0;
			ks->recursion[ks->depth].limit = 0xFFFFFFFF;
			ks->element = NULL;
#ifdef KS_JSON_ENABLE_FILLINFO
			ks->fill_idx = -1;
#endif
			goto continue_key_in_object;
		} else
		if(*buff == '[')
		{
			uint32_t offset = ks->recursion[ks->depth].offset;
			// prepare array
			buff++;
			ks->array = ']';
			ks->depth++;
			if(ks->depth == KS_JSON_MAX_DEPTH)
			{
				ks->error = KS_JSON_TOO_DEEP;
				goto finished;
			}
			// check for type match
			if(ks->element)
			{
				ks->recursion[ks->depth].tmpl = ks->element;
				if(ks->element->info->base.type < KS_TYPE_LAST_NUMERIC)
					ks->recursion[ks->depth].step = kgstruct_type_size[ks->element->info->base.type];
				else
					ks->recursion[ks->depth].step = ks->element->info->base.size;
				ks->recursion[ks->depth].offset = offset - ks->recursion[ks->depth].step;
				ks->recursion[ks->depth].limit = offset + ks->recursion[ks->depth].step * ks->element->info->base.array;
#ifdef KS_JSON_ENABLE_FILLINFO
				ks->recursion[ks->depth].fill_step = 0;
				ks->recursion[ks->depth].fill_offset = ks->recursion[ks->depth-1].fill_offset;
				ks->recursion[ks->depth].fill_idx = ks->fill_idx;
#endif
			} else
			{
				ks->recursion[ks->depth].tmpl = dummy_object;
				ks->recursion[ks->depth].step = 1;
				ks->recursion[ks->depth].limit = 0;
				ks->element = NULL;
#ifdef KS_JSON_ENABLE_FILLINFO
				ks->fill_idx = -1;
#endif
			}
#ifdef KS_JSON_DEBUG
			printf("array; depth %u\n\n", ks->depth);
#endif
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
		buff = skip_whitespace(ks, buff, end);
		if(buff >= end)
		{
			ks->state = KSTATE_VAL_END;
			return KS_JSON_MORE_DATA;
		}
#ifdef KS_JSON_DEBUG
		printf("got val: '%s'\n", ks->str);
#endif

		// process this value
		if(ks->element)
		{
			uint32_t offset = ks->recursion[ks->depth].offset + ks->element->offset;
#ifdef KS_JSON_DEBUG
			printf("assign; offset %u; type %u; jtype %u\n", offset, ks->element->info->base.type, ks->val_type);
#endif
#ifdef KS_JSON_ENABLE_FILLINFO
			was_parsed = 0;
#endif
			if(ks->element->info->base.type == KS_TYPEDEF_STRING)
			{
				// TODO: check type? force only string ?
#ifdef KS_JSON_DEBUG
				printf("* string\n");
#endif
				// check string length
				uint32_t length = ks->ptr - ks->str;
				if(length > ks->element->info->base.size-1)
					 ks->str[ks->element->info->base.size-1] = 0;
				// copy the string
				strcpy((void*)ks->data + offset, (void*)ks->str);
#ifdef KS_JSON_ENABLE_FILLINFO
				was_parsed = 1;
#endif
			} else
#ifdef KGSTRUCT_ENABLE_CUSTOM_TYPE
			if(ks->element->info->base.type == KS_TYPEDEF_CUSTOM)
			{
#ifdef KS_JSON_ENABLE_FILLINFO
				was_parsed =
#endif
					ks->element->info->custom.parse(ks->data + offset, ks->str, ks->val_type == JTYPE_STRING);
			} else
#endif
#ifdef KGSTRUCT_ENABLE_FLAGS
			if(ks->element->info->base.type > KS_TYPEDEF_FLAGS)
			{
				if(ks->val_type == JTYPE_OTHER)
				{
					kgstruct_number_t *dst = ks->data + offset;

					if(!strcmp((void*)ks->str, "true"))
					{
						switch(ks->element->info->base.type)
						{
							case KS_TYPEDEF_FLAG8:
								dst->u8 |= ks->element->flag_bits;
							break;
							case KS_TYPEDEF_FLAG16:
								dst->u16 |= ks->element->flag_bits;
							break;
							case KS_TYPEDEF_FLAG32:
								dst->u32 |= ks->element->flag_bits;
							break;
#ifdef KGSTRUCT_ENABLE_US64
							case KS_TYPEDEF_FLAG64:
								dst->u64 |= ks->element->flag_bits;
							break;
#endif
						}
					} else
					if(!strcmp((void*)ks->str, "false"))
					{
						switch(ks->element->info->base.type)
						{
							case KS_TYPEDEF_FLAG8:
								dst->u8 &= ~ks->element->flag_bits;
							break;
							case KS_TYPEDEF_FLAG16:
								dst->u16 &= ~ks->element->flag_bits;
							break;
							case KS_TYPEDEF_FLAG32:
								dst->u32 &= ~ks->element->flag_bits;
							break;
#ifdef KGSTRUCT_ENABLE_US64
							case KS_TYPEDEF_FLAG64:
								dst->u64 &= ~ks->element->flag_bits;
							break;
#endif
						}
					}
				}
			} else
#endif
#ifndef KS_JSON_ALLOW_STRING_NUMBERS
			if(ks->val_type == JTYPE_OTHER)
#endif
			{
#ifdef KS_JSON_DEBUG
				printf("* value\n");
#endif
				int neg_bad; // -1 = bad; 0 = positive; 1 = negative
				uint8_t *ptr;
				kgstruct_number_t val;
				kgstruct_number_t *dst = ks->data + offset;
				// boolean check
				if(!strcmp((void*)ks->str, "true"))
				{
					val.uval = 1;
					neg_bad = 0;
				} else
				if(!strcmp((void*)ks->str, "false"))
				{
					val.uval = 0;
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

					ptr = get_unsigned(ptr, &val.uval);
					if(!ptr)
						neg_bad = -1;
				}
				// assign
				if(neg_bad >= 0)
				{
					switch(ks->element->info->base.type)
					{
#ifdef KGSTRUCT_ENABLE_FLOAT
						case KS_TYPEDEF_FLOAT:
						{
							if(neg_bad)
								val.f32 = -val.sval;
							else
								val.f32 = val.sval;

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
									val.f32 -= dec;
								else
									val.f32 += dec;
							}
#ifdef KGSTRUCT_ENABLE_MINMAX
#ifdef KS_JSON_ENABLE_FILLINFO
							was_parsed =
#endif
								kgstruct_handle_value(dst, val, ks->element->info, 0);
#else
							dst->f32 = val->f32;
#endif
						}
						break;
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
						case KS_TYPEDEF_DOUBLE:
						{
							if(neg_bad)
								val.f64 = -val.sval;
							else
								val.f64 = val.sval;

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
									val.f64 -= dec;
								else
									val.f64 += dec;
							}
#ifdef KGSTRUCT_ENABLE_MINMAX
#ifdef KS_JSON_ENABLE_FILLINFO
							was_parsed =
#endif
								kgstruct_handle_value(dst, val, ks->element->info, 0);
#else
							dst->f64 = val->f64;
#endif
						}
						break;
#endif
						default:
#ifdef KS_JSON_ENABLE_FILLINFO
							was_parsed =
#endif
								kgstruct_handle_value(dst, val, ks->element->info, neg_bad);
						break;
					}

				}
			}

#ifdef KS_JSON_ENABLE_FILLINFO
			if(was_parsed || ks->array == ']')
				update_fillinfo(ks, ks->depth, 1);
#endif
		}

#ifdef KS_JSON_ENABLE_FILLINFO
		if(ks->array == ']')
		{
			const ks_template_t *element = ks->recursion[ks->depth].tmpl;
			if(element && element->info && element->info->base.type == KS_TYPEDEF_STRUCT)
				ks->recursion[ks->depth].fill_step += element->info->object.basetemp->fill_size;
		}
#endif

skip_empty_object:
		while(*buff == ks->array)
		{
			if(!ks->depth)
			{
				// finished
#ifdef KS_JSON_DEBUG
				printf("FINISHED\n");
#endif
				ks->error = KS_JSON_OK;
				goto finished;
			}
#ifdef KS_JSON_DEBUG
			printf("exit %c; depth %u\n", ks->array, ks->depth);
#endif
			ks->depth--;
			if(ks->recursion[ks->depth].step)
			{
				ks->array = ']';
				ks->element = ks->recursion[ks->depth].tmpl;
#ifdef KS_JSON_ENABLE_FILLINFO
				ks->fill_idx = ks->recursion[ks->depth].fill_idx;
				if(ks->element && ks->element->info && ks->element->info->base.type == KS_TYPEDEF_STRUCT)
					ks->recursion[ks->depth].fill_step += ks->element->info->object.basetemp->fill_size;
#endif
			} else
				ks->array = '}';
			buff++;
			// skip whitespaces
continue_termination:
			buff = skip_whitespace(ks, buff, end);
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
#endif

#ifdef KS_JSON_EXPORTER
uint32_t ks_json_export(kgstruct_json_t *ks, uint8_t *buff, uint32_t length)
{
	uint8_t *end;

	if(!length)
		return 0;

	end = buff + length;

	while(1)
	{
		if(!ks->export_step)
			return length - (uint32_t)(end - buff);

		if(ks->val_type)
		{
			*buff++ = '\t';
			ks->val_type--;
		} else
			buff = ks->export_step(ks, buff, end);

		if(buff >= end)
			return length;
	}
}
#endif

#ifdef KS_JSON_ENABLE_FILLINFO
void ks_json_init(kgstruct_json_t *ks, const ks_base_template_t *basetemp, void *buffer, void *fillinfo)
#else
void ks_json_init(kgstruct_json_t *ks, const ks_base_template_t *basetemp, void *buffer)
#endif
{
	ks->data = buffer;
#ifdef KS_JSON_ENABLE_FILLINFO
	ks->fillinfo = fillinfo;
	ks->recursion[0].fill_offset = 0;
	ks->recursion[0].fill_step = 0;
	ks->recursion[0].fill_idx = 0;
#endif
	ks->recursion[0].tmpl = basetemp->tmpl;
	ks->recursion[0].offset = 0;
	ks->recursion[0].step = 0;
	ks->recursion[0].limit = 0xFFFFFFFF;
	ks->state = KSTATE_START_OBJECT;
	ks->depth = 0;
	ks->error = 0;
	ks->escaped = 0;
	ks->val_type = 0;
	ks->array = '}';
	ks->element = NULL;
#ifdef KS_JSON_LINE_COUNTER
	ks->line = 1;
#endif
#ifdef KS_JSON_EXPORTER
	ks->export_step = export_object_entry;
#endif
}

