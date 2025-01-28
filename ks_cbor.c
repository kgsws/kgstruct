#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#ifdef KGSTRUCT_EXTERNAL_CONFIG
#include KGSTRUCT_EXTERNAL_CONFIG
#endif
#include "kgstruct.h"
#include "ks_cbor.h"

//#define KS_CBOR_DEBUG

#ifdef KS_CBOR_EXPORTER
#error CBOR exporter is not implemented! 
#endif

#define CBOR_TYPE_MASK	0b11100000
#define CBOR_VALUE_MASK	0b00011111

enum
{
	// CBOR types
	CT_INT_POS = 0b00000000,
	CT_INT_NEG = 0b00100000,
	CT_DATA_BINARY = 0b01000000,
	CT_DATA_STRING = 0b01100000,
	CT_LIST = 0b10000000,
	CT_DICT = 0b10100000,
	CT_TAG = 0b11000000, // unsupported
	CT_EXTRA = 0b11100000,
	// CBOR extra types
	CT_FALSE = 20,
	CT_TRUE,
	CT_NULL, // ignored
	CT_UNDEF, // ignored
	CT__RESERVED,
	CT_F16, // unsupported
	CT_F32,
	CT_F64,
};

//
// importer
static int inp_object(kgstruct_cbor_t *ks);
static int inp_array(kgstruct_cbor_t *ks);

static const ks_template_t *find_key(kgstruct_cbor_t *ks)
{
	const ks_template_t *tmpl = ks->recursion[ks->depth].tmpl;

	if(!tmpl)
		return NULL;

	while(tmpl->key)
	{
		if(	tmpl->kln == ks->key_len &&
			!memcmp(tmpl->key, ks->key, ks->key_len)
		){
			if(tmpl->info->base.type == KS_TYPEDEF_CUSTOM)
				return NULL; // TODO: support custom type
			return tmpl;
		}
		tmpl++;
	}

	return NULL;
}

static int inp_base(kgstruct_cbor_t *ks)
{
	return ks->recursion[ks->depth].base_inp(ks);
}

static int inp_value(kgstruct_cbor_t *ks)
{
	while(1)
	{
		if(ks->buff >= ks->end)
			return KS_CBOR_MORE_DATA;

		ks->value.raw[ks->val_cnt] = *ks->buff++;

		if(!ks->val_cnt)
		{
			ks->step_inp = ks->next_func;
			break;
		}

		ks->val_cnt--;
	}

	return 0;
}

static int handle_value(kgstruct_cbor_t *ks, uint8_t in, int (*next)(kgstruct_cbor_t*))
{
#ifdef KGSTRUCT_ENABLE_US64
	if(in > 27)
#else
	if(in >= 27)
#endif
	{
#ifdef KS_CBOR_DEBUG
		printf(" unsupported value %u\n", in);
#endif
		return KS_CBOR_UNSUPPORTED;
	}

	if(in < 24)
	{
		ks->value.uread = in;
		return next(ks);
	}

	ks->val_cnt = (1 << (in - 24)) - 1;
	ks->value.uread = 0;
	ks->step_inp = inp_value;
	ks->next_func = next;

	return 0;
}

static int inp_reader(kgstruct_cbor_t *ks)
{
	while(ks->read_cnt)
	{
		if(ks->buff >= ks->end)
			return KS_CBOR_MORE_DATA;

		*ks->ptr++ = *ks->buff++;
		ks->read_cnt--;
	}

	while(ks->skip_cnt)
	{
		if(ks->buff >= ks->end)
			return KS_CBOR_MORE_DATA;

		ks->buff++;
		ks->skip_cnt--;
	}

	ks->step_inp = ks->next_func;

	return 0;
}

static int handle_copy(kgstruct_cbor_t *ks, uint8_t *dst, uint32_t len, int (*next)(kgstruct_cbor_t*))
{
	if(dst)
	{
		ks->ptr = dst;
		ks->read_cnt = len;
		ks->skip_cnt = 0;
	} else
	{
		ks->read_cnt = 0;
		ks->skip_cnt = len;
	}

	ks->step_inp = inp_reader;
	ks->next_func = next;

	return 0;
}

static void copy_value(uint8_t *dst, uint8_t *src, uint32_t type)
{
	switch(type)
	{
#ifdef KGSTRUCT_ENABLE_US64
		case KS_TYPEDEF_FLAG64:
		case KS_TYPEDEF_U64:
		case KS_TYPEDEF_S64:
			*dst++ = *src++;
			*dst++ = *src++;
			*dst++ = *src++;
			*dst++ = *src++;
#endif
		case KS_TYPEDEF_FLAG32:
		case KS_TYPEDEF_U32:
		case KS_TYPEDEF_S32:
			*dst++ = *src++;
			*dst++ = *src++;
		case KS_TYPEDEF_FLAG16:
		case KS_TYPEDEF_U16:
		case KS_TYPEDEF_S16:
			*dst++ = *src++;
		case KS_TYPEDEF_FLAG8:
		case KS_TYPEDEF_U8:
		case KS_TYPEDEF_S8:
			*dst++ = *src++;
		break;
	}
}

static void *get_dest(kgstruct_cbor_t *ks, const ks_template_t *tmpl)
{
	kgstruct_cbor_recursion_t *r = ks->recursion + ks->depth;
	uint8_t *dst;

	dst = r->dest;
	dst += tmpl->offset;

	return dst;
}

static int inp_val_int(kgstruct_cbor_t *ks)
{
	const ks_template_t *tmpl;

	tmpl = ks->recursion[ks->depth].ktpl;
	if(tmpl)
	{
		uint32_t type = tmpl->info->base.type;
		if(type < KS_TYPE_LAST_INTEGER)
		{
			uint_fast8_t is_neg = ks->val_type == CT_INT_NEG;
			kgstruct_number_t *dst = get_dest(ks, tmpl);
			if(is_neg)
				ks->value.uread++;
			kgstruct_handle_value(dst, ks->value, tmpl->info, is_neg);
		}
	}

	ks->step_inp = inp_base;

	return 0;
}

#if defined(KGSTRUCT_ENABLE_FLOAT) || defined(KGSTRUCT_ENABLE_DOUBLE)
static int inp_val_flt(kgstruct_cbor_t *ks)
{
	const ks_template_t *tmpl;

	tmpl = ks->recursion[ks->depth].ktpl;
	if(tmpl)
	{
		kgstruct_number_t *dst = get_dest(ks, tmpl);
		uint32_t type = tmpl->info->base.type;
#ifdef KGSTRUCT_ENABLE_MINMAX
		if(
#ifdef KGSTRUCT_ENABLE_FLOAT
			(ks->val_type == CT_F32 && type == KS_TYPEDEF_FLOAT) ||
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
			(ks->val_type == CT_F64 && type == KS_TYPEDEF_DOUBLE) ||
#endif
		0)
			kgstruct_handle_value(dst, ks->value, tmpl->info, 0);
#else
#ifdef KGSTRUCT_ENABLE_FLOAT
		if(ks->val_type == CT_F32)
			dst->f32 = ks->value.f32;
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
		if(ks->val_type == CT_F64)
			dst->f64 = ks->value.f64;
#endif
#endif
	}

	return inp_base(ks);
}
#endif

static int inp_val_data(kgstruct_cbor_t *ks)
{
	const ks_template_t *tmpl;
#ifdef KS_CBOR_DEBUG
	printf("  str / bin %u\n", ks->value.u32);
#endif
	tmpl = ks->recursion[ks->depth].ktpl;
	if(	tmpl &&
		(
			tmpl->info->base.type == KS_TYPEDEF_STRING ||
			tmpl->info->base.type == KS_TYPEDEF_BINARY
		)
	){
		uint32_t tmp = tmpl->info->base.size;

		ks->ptr = ks->recursion[ks->depth].dest + tmpl->offset;

		if(ks->val_type == CT_DATA_STRING)
			tmp--;

		if(tmp > ks->value.u32)
			tmp = ks->value.u32;

		if(ks->val_type == CT_DATA_STRING)
			ks->ptr[tmp] = 0;

		ks->read_cnt = tmp;
		ks->skip_cnt = ks->value.u32 - tmp;
	} else
	{
		ks->read_cnt = 0;
		ks->skip_cnt = ks->value.u32;
	}

	ks->step_inp = inp_reader;
	ks->next_func = inp_base;
#ifdef KS_CBOR_DEBUG
	printf("  read %u skip %u\n", ks->read_cnt, ks->skip_cnt);
#endif
	return 0;
}

static int inp_entry(kgstruct_cbor_t *ks)
{
	const ks_template_t *tmpl;
	uint8_t in;

	if(ks->buff >= ks->end)
		return KS_CBOR_MORE_DATA;

	in = *ks->buff++;
	ks->val_type = in & CBOR_TYPE_MASK;
	tmpl = ks->recursion[ks->depth].ktpl;

#ifdef KS_CBOR_DEBUG
	if(ks->recursion[ks->depth].asize)
		printf(" index: %u; %p\n", ks->recursion[ks->depth].idx, tmpl);
	else
		printf(" key: %.*s; %p\n", ks->key_len, ks->key, tmpl);
	printf(" type %u\n", in >> 5);
#endif

	switch(in & CBOR_TYPE_MASK)
	{
		case CT_INT_POS:
		case CT_INT_NEG:
			return handle_value(ks, in & CBOR_VALUE_MASK, inp_val_int);
		break;
		case CT_DATA_BINARY:
		case CT_DATA_STRING:
			return handle_value(ks, in & CBOR_VALUE_MASK, inp_val_data);
		case CT_LIST:
		{
			kgstruct_cbor_recursion_t *r = ks->recursion + ks->depth;

			if(ks->depth >= KS_CBOR_MAX_DEPTH)
				return KS_CBOR_TOO_DEEP;

			r[1].asize = -1;

			if(tmpl)
			{
				if(tmpl->info->base.array)
				{
					r[1].asize = tmpl->info->base.size;
					r[1].amax = tmpl->info->base.array;
				} else
					tmpl = NULL;
			}

			r[1].dest = r[0].dest;
			r[1].tmpl = tmpl;
			r[1].ktpl = tmpl;

			ks->depth++;
#ifdef KS_CBOR_DEBUG
			printf(" enter list: %u %p\n", ks->depth, tmpl);
#endif
			ks->buff--;
			ks->step_inp = inp_array;
		}
		break;
		case CT_DICT:
		{
			kgstruct_cbor_recursion_t *r = ks->recursion + ks->depth;

			if(ks->depth >= KS_CBOR_MAX_DEPTH)
				return KS_CBOR_TOO_DEEP;

			if(	tmpl &&
				(
					tmpl->info->base.type != KS_TYPEDEF_STRUCT &&
					tmpl->info->base.type != KS_TYPEDEF_FLAGS
				)
			)
				tmpl = NULL;
			if(tmpl)
			{
				r[1].dest = r[0].dest + tmpl->offset;
				r[1].tmpl = tmpl->info->object.basetemp->tmpl;
			} else
				r[1].tmpl = NULL;
			r[1].asize = 0;

			ks->depth++;
#ifdef KS_CBOR_DEBUG
			printf(" enter dict: %u %p\n", ks->depth, r[1].tmpl);
#endif
			ks->buff--;
			ks->step_inp = inp_object;
		}
		break;
		case CT_EXTRA:
#ifdef KS_CBOR_DEBUG
			printf(" ext %u\n", in & CBOR_VALUE_MASK);
#endif
			ks->val_type = in & CBOR_VALUE_MASK;
			switch(ks->val_type)
			{
				case CT_FALSE:
				case CT_TRUE:
					if(tmpl)
					{
						uint8_t *p0 = get_dest(ks, tmpl);
						uint8_t *p1 = ks->value.raw;
						uint32_t type = tmpl->info->base.type;

						switch(type)
						{
							case KS_TYPEDEF_FLAG8:
							case KS_TYPEDEF_FLAG16:
							case KS_TYPEDEF_FLAG32:
#ifdef KGSTRUCT_ENABLE_US64
							case KS_TYPEDEF_FLAG64:
#endif
								copy_value(p1, p0, type);

								if(in & 1)
									ks->value.uread |= tmpl->flag_bits;
								else
									ks->value.uread &= ~tmpl->flag_bits;

								copy_value(p0, p1, type);
							break;
#ifdef KS_CBOR_BOOL_AS_INTEGER
							case KS_TYPEDEF_S8:
							case KS_TYPEDEF_U8:
							case KS_TYPEDEF_S16:
							case KS_TYPEDEF_U16:
							case KS_TYPEDEF_S32:
							case KS_TYPEDEF_U32:
#ifdef KGSTRUCT_ENABLE_US64
							case KS_TYPEDEF_S64:
							case KS_TYPEDEF_U64:
#endif
								ks->value.uread = in & 1;
								copy_value(p0, p1, type);
							break;
#endif
						}
					}
					ks->step_inp = inp_base;
				break;
#ifdef KGSTRUCT_ENABLE_FLOAT
				case CT_F32:
					ks->val_cnt = sizeof(float) - 1;
					ks->value.uread = 0;
					ks->step_inp = inp_value;
					ks->next_func = inp_val_flt;
					return 0;
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
				case CT_F64:
					ks->val_cnt = sizeof(double) - 1;
					ks->value.uread = 0;
					ks->step_inp = inp_value;
					ks->next_func = inp_val_flt;
					return 0;
#endif
				default:
#ifdef KS_CBOR_DEBUG
					printf(" unsupported extended %u\n", in & CBOR_VALUE_MASK);
#endif
					return KS_CBOR_UNSUPPORTED;
			}
		break;
		default:
#ifdef KS_CBOR_DEBUG
			printf(" unsupported type %u\n", in >> 5);
#endif
			return KS_CBOR_UNSUPPORTED;
	}

	return 0;
}

static int inp_val_obj(kgstruct_cbor_t *ks)
{
	ks->key_len = ks->value.u32;
	ks->step_inp = inp_entry;
	ks->recursion[ks->depth].ktpl = find_key(ks);
	return inp_entry(ks);
}

static int inp_key_obj(kgstruct_cbor_t *ks)
{
	if(ks->value.uread >= KS_CBOR_MAX_KEY_LENGTH)
		return KS_CBOR_TOO_LONG_STRING;
#ifdef KS_CBOR_DEBUG
	printf(" size %u\n", ks->value.u32);
#endif
	ks->ptr = ks->key;
	ks->read_cnt = ks->value.u32;
	ks->skip_cnt = 0;
	ks->step_inp = inp_reader;
	ks->next_func = inp_val_obj;

	return 0;
}

static int inp_okey_type(kgstruct_cbor_t *ks)
{
	uint8_t in;

	if(ks->buff >= ks->end)
		return KS_CBOR_MORE_DATA;

	in = *ks->buff++;

	switch(in & CBOR_TYPE_MASK)
	{
		case CT_DATA_STRING:
#ifdef KS_CBOR_DEBUG
			printf("key string (%u)\n", in & CBOR_VALUE_MASK);
#endif
			return handle_value(ks, in & CBOR_VALUE_MASK, inp_key_obj);
		break;
		default:
#ifdef KS_CBOR_DEBUG
			printf("bad key type %u\n", in >> 5);
#endif
			return KS_CBOR_BAD_KEY_TYPE;
	}
}

static int inp_okey(kgstruct_cbor_t *ks)
{
	kgstruct_cbor_recursion_t *r = ks->recursion + ks->depth;

	if(!r->count)
	{
#ifdef KS_CBOR_DEBUG
		printf("leave dict %u\n", ks->depth);
#endif
		if(!ks->depth)
			return -1;
		ks->depth--;
		return 0;
	}

	r->count--;

	ks->step_inp = inp_okey_type;

	return inp_okey_type(ks);
}

static int inp_obj_cnt(kgstruct_cbor_t *ks)
{
	if(ks->value.uread >= 0x80000000)
	{
#ifdef KS_CBOR_DEBUG
		printf(" unsupported size\n");
#endif
		return KS_CBOR_UNSUPPORTED;
	}

	ks->recursion[ks->depth].count = ks->value.u32;
	ks->recursion[ks->depth].base_inp = inp_okey;
	ks->step_inp = inp_okey;
#ifdef KS_CBOR_DEBUG
	printf(" count %u\n", ks->value.u32);
#endif
	return 0;
}

static int inp_object(kgstruct_cbor_t *ks)
{
	uint8_t in;

	if(ks->buff >= ks->end)
		return KS_CBOR_MORE_DATA;

	in = *ks->buff++;

	// check for object
	if((in & CBOR_TYPE_MASK) != CT_DICT)
		return KS_CBOR_NEED_OBJECT;

#ifdef KS_CBOR_DEBUG
	printf("object (%u)\n", in & CBOR_VALUE_MASK);
#endif
	return handle_value(ks, in & CBOR_VALUE_MASK, inp_obj_cnt);
}

static int inp_aval(kgstruct_cbor_t *ks)
{
	uint8_t in;
	kgstruct_cbor_recursion_t *r = ks->recursion + ks->depth;

	if(!r->count)
	{
#ifdef KS_CBOR_DEBUG
		printf("leave list %u\n", ks->depth);
#endif
		ks->depth--;
		r--;
		ks->step_inp = inp_base;
		return 0;
	}

	r->count--;
	r->idx++;
	r->dest += r->asize;

	if(r->tmpl && r->idx >= r->amax)
	{
		r->tmpl = NULL;
		r->ktpl = NULL;
	}

	ks->step_inp = inp_entry;

	return inp_entry(ks);
}

static int inp_arr_cnt(kgstruct_cbor_t *ks)
{
	kgstruct_cbor_recursion_t *r = ks->recursion + ks->depth;

	if(ks->value.uread >= 0x80000000)
	{
#ifdef KS_CBOR_DEBUG
		printf(" unsupported size\n");
#endif
		return KS_CBOR_UNSUPPORTED;
	}

	r->dest -= r->asize;
	r->idx = -1;
	r->count = ks->value.u32;
	r->base_inp = inp_aval;
	ks->step_inp = inp_aval;
#ifdef KS_CBOR_DEBUG
	printf(" count %u\n", ks->value.u32);
#endif
	return 0;
}

static int inp_array(kgstruct_cbor_t *ks)
{
	uint8_t in;

	if(ks->buff >= ks->end)
		return KS_CBOR_MORE_DATA;

	in = *ks->buff++;

	// check for array
	if((in & CBOR_TYPE_MASK) != CT_LIST)
		return KS_CBOR_NEED_ARRAY;

#ifdef KS_CBOR_DEBUG
	printf("array (%u)\n", in & CBOR_VALUE_MASK);
#endif
	return handle_value(ks, in & CBOR_VALUE_MASK, inp_arr_cnt);
}

//
// API

#ifdef KS_CBOR_PARSER
int ks_cbor_parse(kgstruct_cbor_t *ks, const uint8_t *buff, uint32_t length)
{
	int ret;

	ks->buff = buff;
	ks->end = buff + length;

	do
	{
		ret = ks->step_inp(ks);
	} while(!ret);

	if(ret < 0)
		return 0;

	return ret;
}
#endif

#ifdef KS_CBOR_ENABLE_FILLINFO
#error CBOR fill-info is not implemented!
void ks_cbor_init(kgstruct_cbor_t *ks, const ks_base_template_t *basetemp, void *buffer, void *fillinfo)
#else
void ks_cbor_init(kgstruct_cbor_t *ks, const ks_base_template_t *basetemp, void *buffer)
#endif
{
	ks->depth = 0;
	ks->step_inp = inp_object;

	ks->recursion[0].dest = buffer;
	ks->recursion[0].tmpl = basetemp->tmpl;
}
