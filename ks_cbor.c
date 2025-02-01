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
// common

static void *get_pointer(kgstruct_cbor_t *ks, const ks_template_t *tmpl)
{
	kgstruct_cbor_recursion_t *r = ks->recursion + ks->depth;
	uint8_t *ptr;

	ptr = r->buff;
	ptr += tmpl->offset;

	return ptr;
}

#ifdef KS_CBOR_EXPORTER
////
//// exporter
////

static int exp_object(kgstruct_cbor_t *ks);
static int exp_obj_val(kgstruct_cbor_t *ks);

static int exp_value(kgstruct_cbor_t *ks)
{
	while(1)
	{
		if(ks->buff >= ks->end)
			return ks->buff - ks->top;

		*ks->buff++ = ks->value.raw[ks->val_cnt];

		if(!ks->val_cnt)
		{
			ks->step = ks->next_func;
			return 0;
		}

		ks->val_cnt--;
	}

	return 0;
}

static int exp_val_ent(kgstruct_cbor_t *ks)
{
	uint8_t type;

	if(ks->buff >= ks->end)
		return ks->buff - ks->top;

	if(ks->value.uval < 24)
	{
		*ks->buff++ = ks->val_type | ks->value.u8;
		ks->step = ks->next_func;
		return 0;
	} else
	if(ks->value.uval < 256)
	{
		ks->val_cnt = 0;
		type = 24;
	} else
	if(ks->value.uval < 65536)
	{
		ks->val_cnt = 1;
		type = 25;
	} else
#ifdef KGSTRUCT_ENABLE_US64
	if(ks->value.uval > 4294967295)
	{
		ks->val_cnt = 7;
		type = 27;
	} else
#endif
	{
		ks->val_cnt = 3;
		type = 26;
	}

	*ks->buff++ = ks->val_type | type;
	ks->step = exp_value;

	return 0;
}

static int exp_writer(kgstruct_cbor_t *ks)
{
	while(ks->byte_cnt)
	{
		if(ks->buff >= ks->end)
			return ks->buff - ks->top;

		*ks->buff++ = *ks->ptr++;
		ks->byte_cnt--;
	}

	ks->step = ks->next_func;

	return 0;
}

static int exp_extended(kgstruct_cbor_t *ks)
{
	if(ks->buff >= ks->end)
		return ks->buff - ks->top;

	*ks->buff++ = CT_EXTRA | ks->val_type;

	if(ks->byte_cnt)
	{
		ks->ptr = ks->value.raw;
		ks->step = exp_writer;
		return 0;
	}

	ks->step = ks->next_func;

	return 0;
}

static int manage_value(kgstruct_cbor_t *ks, uint32_t type, kgstruct_uint_t uval, int (*cb)(kgstruct_cbor_t*))
{
	ks->val_type = type;
	ks->value.uval = uval;
	ks->step = exp_val_ent;
	ks->next_func = cb;
	return exp_val_ent(ks);
}

static int manage_signed(kgstruct_cbor_t *ks, kgstruct_int_t sval, int (*cb)(kgstruct_cbor_t*))
{
	uint32_t type = CT_INT_POS;
	kgstruct_uint_t uval = sval;

	if(sval < 0)
	{
		uval = -sval;
		uval--;
		type = CT_INT_NEG;
	}

	return manage_value(ks, type, uval, cb);
}

static int manage_extended(kgstruct_cbor_t *ks, uint32_t type, kgstruct_number_t *val, int (*cb)(kgstruct_cbor_t*))
{
	switch(type)
	{
#ifdef KGSTRUCT_ENABLE_FLOAT
		case CT_F32:
			for(uint32_t i = 0; i < sizeof(float); i++)
				ks->value.raw[i] = val->raw[sizeof(float)-1-i];
			ks->byte_cnt = sizeof(float);
		break;
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
		case CT_F64:
			for(uint32_t i = 0; i < sizeof(double); i++)
				ks->value.raw[i] = val->raw[sizeof(double)-1-i];
			ks->byte_cnt = sizeof(double);
		break;
#endif
		default:
			type = CT_UNDEF;
			__attribute__((fallthrough));
		case CT_FALSE:
		case CT_TRUE:
			ks->byte_cnt = 0;
		break;
	}

	ks->val_type = type;
	ks->step = exp_extended;
	ks->next_func = cb;

	return exp_extended(ks);
}

static int exp_obj_base(kgstruct_cbor_t *ks)
{
	return ks->recursion[ks->depth].base_func(ks);
}

static int exp_data(kgstruct_cbor_t *ks)
{
	ks->byte_cnt = ks->value.uval;
	ks->step = exp_writer;
	ks->next_func = exp_obj_base;
	return 0;
}

static int exp_array(kgstruct_cbor_t *ks)
{
	kgstruct_cbor_recursion_t *r = ks->recursion + ks->depth;

	if(!r->count)
	{
		ks->depth--;
		ks->step = exp_obj_base;
		return 0;
	}

	r->count--;
	r->buff += r->asize;

	ks->step = exp_obj_val;

	return 0;
}

static int exp_obj_val(kgstruct_cbor_t *ks)
{
	kgstruct_cbor_recursion_t *r = ks->recursion + ks->depth;
	const ks_template_t *tmpl = r->ktpl;
	kgstruct_number_t *val = get_pointer(ks, tmpl);
	const kgstruct_type_t *info = tmpl->info;
#ifdef KGSTRUCT_ENABLE_FLAGS
	kgstruct_uint_t flags;
#endif

	if(info->base.array && !r->asize)
	{
		r[1].base = r[0].base;
		r[1].ktpl = r[0].ktpl;
		r[1].buff = r[0].buff - info->base.size;
		r[1].count = info->base.array;
		r[1].asize = info->base.size;
		r[1].base_func = exp_array;
		ks->depth++;
		return manage_value(ks, CT_LIST, info->base.array, exp_array);
	}

	switch(info->base.type)
	{
		case KS_TYPEDEF_U8:
			return manage_value(ks, CT_INT_POS, val->u8, exp_obj_base);
		case KS_TYPEDEF_S8:
			return manage_signed(ks, val->s8, exp_obj_base);
		case KS_TYPEDEF_U16:
			return manage_value(ks, CT_INT_POS, val->u16, exp_obj_base);
		case KS_TYPEDEF_S16:
			return manage_signed(ks, val->s16, exp_obj_base);
		case KS_TYPEDEF_U32:
			return manage_value(ks, CT_INT_POS, val->u32, exp_obj_base);
		case KS_TYPEDEF_S32:
			return manage_signed(ks, val->s32, exp_obj_base);
#ifdef KGSTRUCT_ENABLE_US64
		case KS_TYPEDEF_U64:
			return manage_value(ks, CT_INT_POS, val->u64, exp_obj_base);
		case KS_TYPEDEF_S64:
			return manage_signed(ks, val->s64, exp_obj_base);
#endif
#ifdef KGSTRUCT_ENABLE_FLOAT
		case KS_TYPEDEF_FLOAT:
			return manage_extended(ks, CT_F32, val, exp_obj_base);
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
		case KS_TYPEDEF_DOUBLE:
			return manage_extended(ks, CT_F64, val, exp_obj_base);
#endif
		case KS_TYPEDEF_STRING:
			ks->ptr = (uint8_t*)val;
			return manage_value(ks, CT_DATA_STRING, strlen((char*)val), exp_data);
		case KS_TYPEDEF_BINARY:
			ks->ptr = (uint8_t*)val;
			return manage_value(ks, CT_DATA_BINARY, info->base.size, exp_data);
		break;
		case KS_TYPEDEF_STRUCT:
		case KS_TYPEDEF_FLAGS:
			r[1].base = info->object.basetemp;
			r[1].buff = r[0].buff + tmpl->offset;
			r[1].count = r[1].base->count;
			r[1].asize = 0;
			ks->depth++;
			ks->step = exp_object;
		break;
#ifdef KGSTRUCT_ENABLE_FLAGS
		case KS_TYPEDEF_FLAG8:
			flags = val->u8;
do_flags:
			if(flags & tmpl->flag_bits)
				flags = CT_TRUE;
			else
				flags = CT_FALSE;
			return manage_extended(ks, flags, NULL, exp_obj_base);
		case KS_TYPEDEF_FLAG16:
			flags = val->u16;
			goto do_flags;
		case KS_TYPEDEF_FLAG32:
			flags = val->u32;
			goto do_flags;
#ifdef KGSTRUCT_ENABLE_US64
		case KS_TYPEDEF_FLAG64:
			flags = val->u64;
			goto do_flags;
#endif
#endif
		default:
			return manage_extended(ks, CT_UNDEF, NULL, exp_obj_base);
	}

	return 0;
}

static int exp_obj_str(kgstruct_cbor_t *ks)
{
	ks->ptr = (uint8_t*)ks->recursion[ks->depth].ktpl->key;
	ks->byte_cnt = ks->value.uval;
	ks->step = exp_writer;
	ks->next_func = exp_obj_val;
	return 0;
}

static int exp_obj_key(kgstruct_cbor_t *ks)
{
	kgstruct_cbor_recursion_t *r = ks->recursion + ks->depth;

	if(!r->count)
	{
		if(!ks->depth)
		{
			if(ks->buff == ks->top)
				return -1;
			return ks->buff - ks->top;
		}
		ks->depth--;
		ks->step = exp_obj_base;
		return 0;
	}

	r->count--;
	r->ktpl++;

	return manage_value(ks, CT_DATA_STRING, r->ktpl->kln, exp_obj_str);
}

static int exp_object(kgstruct_cbor_t *ks)
{
	kgstruct_cbor_recursion_t *r = ks->recursion + ks->depth;

	r->ktpl = r->base->tmpl;
	r->ktpl--;

	r->base_func = exp_obj_key;

	return manage_value(ks, CT_DICT, r->base->count, exp_obj_key);
}

//
// API

void ks_cbor_init_export(kgstruct_cbor_t *ks, const ks_base_template_t *basetemp, void *buffer)
{
	ks->step = exp_object;
	ks->depth = 0;
	ks->recursion[0].buff = buffer;
	ks->recursion[0].base = basetemp;
	ks->recursion[0].count = basetemp->count;
	ks->recursion[0].asize = 0;
}

#endif

#ifdef KS_CBOR_IMPORTER

////
//// importer
////

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
#ifdef KGSTRUCT_ENABLE_CUSTOM_TYPE
			if(tmpl->info->base.type == KS_TYPEDEF_CUSTOM)
				return NULL; // TODO: support custom type
#endif
			return tmpl;
		}
		tmpl++;
	}

	return NULL;
}

static int inp_base(kgstruct_cbor_t *ks)
{
	return ks->recursion[ks->depth].base_func(ks);
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
			ks->step = ks->next_func;
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
		ks->value.uval = in;
		return next(ks);
	}

	ks->val_cnt = (1 << (in - 24)) - 1;
	ks->value.uval = 0;
	ks->step = inp_value;
	ks->next_func = next;

	return 0;
}

static int inp_reader(kgstruct_cbor_t *ks)
{
	while(ks->byte_cnt)
	{
		if(ks->buff >= ks->end)
			return KS_CBOR_MORE_DATA;

		*ks->ptr++ = *ks->buff++;
		ks->byte_cnt--;
	}

	while(ks->skip_cnt)
	{
		if(ks->buff >= ks->end)
			return KS_CBOR_MORE_DATA;

		ks->buff++;
		ks->skip_cnt--;
	}

	ks->step = ks->next_func;

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
			__attribute__((fallthrough));
#endif
		case KS_TYPEDEF_FLAG32:
		case KS_TYPEDEF_U32:
		case KS_TYPEDEF_S32:
			*dst++ = *src++;
			*dst++ = *src++;
			__attribute__((fallthrough));
		case KS_TYPEDEF_FLAG16:
		case KS_TYPEDEF_U16:
		case KS_TYPEDEF_S16:
			*dst++ = *src++;
			__attribute__((fallthrough));
		case KS_TYPEDEF_FLAG8:
		case KS_TYPEDEF_U8:
		case KS_TYPEDEF_S8:
			*dst++ = *src++;
		break;
	}
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
			kgstruct_number_t *dst = get_pointer(ks, tmpl);
			if(is_neg)
				ks->value.uval++;
			kgstruct_handle_value(dst, ks->value, tmpl->info, is_neg);
		}
	}

	ks->step = inp_base;

	return 0;
}

#if defined(KGSTRUCT_ENABLE_FLOAT) || defined(KGSTRUCT_ENABLE_DOUBLE)
static int inp_val_flt(kgstruct_cbor_t *ks)
{
	const ks_template_t *tmpl;

	tmpl = ks->recursion[ks->depth].ktpl;
	if(tmpl)
	{
		kgstruct_number_t *dst = get_pointer(ks, tmpl);
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

		ks->ptr = ks->recursion[ks->depth].buff + tmpl->offset;

		if(ks->val_type == CT_DATA_STRING)
			tmp--;

		if(tmp > ks->value.u32)
			tmp = ks->value.u32;

		if(ks->val_type == CT_DATA_STRING)
			ks->ptr[tmp] = 0;

		ks->byte_cnt = tmp;
		ks->skip_cnt = ks->value.u32 - tmp;
	} else
	{
		ks->byte_cnt = 0;
		ks->skip_cnt = ks->value.u32;
	}

	ks->step = inp_reader;
	ks->next_func = inp_base;
#ifdef KS_CBOR_DEBUG
	printf("  read %u skip %u\n", ks->byte_cnt, ks->skip_cnt);
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

			r[1].buff = r[0].buff;
			r[1].tmpl = tmpl;
			r[1].ktpl = tmpl;

			ks->depth++;
#ifdef KS_CBOR_DEBUG
			printf(" enter list: %u %p\n", ks->depth, tmpl);
#endif
			ks->buff--;
			ks->step = inp_array;
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
				r[1].buff = r[0].buff + tmpl->offset;
				r[1].tmpl = tmpl->info->object.basetemp->tmpl;
			} else
				r[1].tmpl = NULL;
			r[1].asize = 0;

			ks->depth++;
#ifdef KS_CBOR_DEBUG
			printf(" enter dict: %u %p\n", ks->depth, r[1].tmpl);
#endif
			ks->buff--;
			ks->step = inp_object;
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
						uint8_t *p0 = get_pointer(ks, tmpl);
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
									ks->value.uval |= tmpl->flag_bits;
								else
									ks->value.uval &= ~tmpl->flag_bits;

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
								ks->value.uval = in & 1;
								copy_value(p0, p1, type);
							break;
#endif
						}
					}
					__attribute__((fallthrough));
				case CT_NULL:
				case CT_UNDEF:
					ks->step = inp_base;
				break;
#ifdef KGSTRUCT_ENABLE_FLOAT
				case CT_F32:
					ks->val_cnt = sizeof(float) - 1;
					ks->value.uval = 0;
					ks->step = inp_value;
					ks->next_func = inp_val_flt;
					return 0;
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
				case CT_F64:
					ks->val_cnt = sizeof(double) - 1;
					ks->value.uval = 0;
					ks->step = inp_value;
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
	ks->step = inp_entry;
	ks->recursion[ks->depth].ktpl = find_key(ks);
	return inp_entry(ks);
}

static int inp_key_obj(kgstruct_cbor_t *ks)
{
	if(ks->value.uval >= KS_CBOR_MAX_KEY_LENGTH)
		return KS_CBOR_TOO_LONG_STRING;
#ifdef KS_CBOR_DEBUG
	printf(" size %u\n", ks->value.u32);
#endif
	ks->ptr = ks->key;
	ks->byte_cnt = ks->value.u32;
	ks->skip_cnt = 0;
	ks->step = inp_reader;
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

	ks->step = inp_okey_type;

	return inp_okey_type(ks);
}

static int inp_obj_cnt(kgstruct_cbor_t *ks)
{
	if(ks->value.uval >= 0x80000000)
	{
#ifdef KS_CBOR_DEBUG
		printf(" unsupported size\n");
#endif
		return KS_CBOR_UNSUPPORTED;
	}

	ks->recursion[ks->depth].count = ks->value.u32;
	ks->recursion[ks->depth].base_func = inp_okey;
	ks->step = inp_okey;
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
	kgstruct_cbor_recursion_t *r = ks->recursion + ks->depth;

	if(!r->count)
	{
#ifdef KS_CBOR_DEBUG
		printf("leave list %u\n", ks->depth);
#endif
		ks->depth--;
		r--;
		ks->step = inp_base;
		return 0;
	}

	r->count--;
	r->idx++;
	r->buff += r->asize;

	if(r->tmpl && r->idx >= r->amax)
	{
		r->tmpl = NULL;
		r->ktpl = NULL;
	}

	ks->step = inp_entry;

	return inp_entry(ks);
}

static int inp_arr_cnt(kgstruct_cbor_t *ks)
{
	kgstruct_cbor_recursion_t *r = ks->recursion + ks->depth;

	if(ks->value.uval >= 0x80000000)
	{
#ifdef KS_CBOR_DEBUG
		printf(" unsupported size\n");
#endif
		return KS_CBOR_UNSUPPORTED;
	}

	r->buff -= r->asize;
	r->idx = -1;
	r->count = ks->value.u32;
	r->base_func = inp_aval;
	ks->step = inp_aval;
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

void ks_cbor_init_import(kgstruct_cbor_t *ks, const ks_base_template_t *basetemp, void *buffer)
{
	ks->step = inp_object;
	ks->depth = 0;
	ks->recursion[0].buff = buffer;
	ks->recursion[0].tmpl = basetemp->tmpl;
}

#endif

//
// common API

int ks_cbor_feed(kgstruct_cbor_t *ks, const uint8_t *buff, uint32_t length)
{
	int ret;

#ifdef KS_CBOR_EXPORTER
	ks->top = (uint8_t*)buff;
#endif
	ks->buff = (uint8_t*)buff;
	ks->end = (uint8_t*)buff + length;

	do
	{
		ret = ks->step(ks);
	} while(!ret);

	if(ret < 0)
		return 0;

	return ret;
}

