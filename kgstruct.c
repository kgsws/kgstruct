#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#ifdef KGSTRUCT_EXTERNAL_CONFIG
#include KGSTRUCT_EXTERNAL_CONFIG
#endif
#include "kgstruct.h"

//
// API

int kgstruct_handle_value(kgstruct_number_t *dst, kgstruct_number_t val, const kgstruct_type_t *info, uint32_t is_neg)
{
	if(is_neg)
		val.sval = -val.sval;

	switch(info->base.type)
	{
		//// 8 bit
		case KS_TYPEDEF_U8:
#ifdef KGSTRUCT_ENABLE_MINMAX
			if(info->base.flags & KS_TYPEFLAG_HAS_MIN)
			{
				if(is_neg || val.uval < (kgstruct_uint_t)info->u8.min)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->u8 = info->u8.min;
					return 1;
				}
			} else
			if(is_neg)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->u8 = 0;
				return 1;
			}

			if(info->base.flags & KS_TYPEFLAG_HAS_MAX)
			{
				if(val.uval > (kgstruct_uint_t)info->u8.max)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->u8 = info->u8.max;
					return 1;
				}
			} else
			if(val.uval > 255)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->u8 = 255;
				return 1;
			}
#endif
			dst->u8 = val.u8;
			return 1;
		case KS_TYPEDEF_S8:
#ifdef KGSTRUCT_ENABLE_MINMAX
			if(info->base.flags & KS_TYPEFLAG_HAS_MIN)
			{
				if(val.sval < (kgstruct_int_t)info->s8.min)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->s8 = info->s8.min;
					return 1;
				}
			} else
			if(val.sval < -128)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->s8 = -128;
				return 1;
			}

			if(info->base.flags & KS_TYPEFLAG_HAS_MAX)
			{
				if(val.sval > (kgstruct_int_t)info->s8.max)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->s8 = info->s8.max;
					return 1;
				}
			} else
			if(val.sval > 127)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->s8 = 127;
				return 1;
			}
#endif
			dst->s8 = val.s8;
			return 1;
		//// 16 bit
		case KS_TYPEDEF_U16:
#ifdef KGSTRUCT_ENABLE_MINMAX
			if(info->base.flags & KS_TYPEFLAG_HAS_MIN)
			{
				if(is_neg || val.uval < (kgstruct_uint_t)info->u16.min)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->u16 = info->u16.min;
					return 1;
				}
			} else
			if(is_neg)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->u16 = 0;
				return 1;
			}

			if(info->base.flags & KS_TYPEFLAG_HAS_MAX)
			{
				if(val.uval > (kgstruct_uint_t)info->u16.max)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->u16 = info->u16.max;
					return 1;
				}
			} else
			if(val.uval > 65535)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->u16 = 65535;
				return 1;
			}
#endif
			dst->u16 = val.u16;
			return 1;
		case KS_TYPEDEF_S16:
#ifdef KGSTRUCT_ENABLE_MINMAX
			if(info->base.flags & KS_TYPEFLAG_HAS_MIN)
			{
				if(val.sval < (kgstruct_int_t)info->s16.min)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->s16 = info->s16.min;
					return 1;
				}
			} else
			if(val.sval < -32768)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->s16 = -32768;
				return 1;
			}

			if(info->base.flags & KS_TYPEFLAG_HAS_MAX)
			{
				if(val.sval > (kgstruct_int_t)info->s16.max)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->s16 = info->s16.max;
					return 1;
				}
			} else
			if(val.sval > 32767)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->s16 = 32767;
				return 1;
			}
#endif
			dst->s16 = val.s16;
			return 1;
		//// 32 bit
		case KS_TYPEDEF_U32:
#ifdef KGSTRUCT_ENABLE_MINMAX
			if(info->base.flags & KS_TYPEFLAG_HAS_MIN)
			{
				if(is_neg || val.uval < (kgstruct_uint_t)info->u32.min)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->u32 = info->u32.min;
					return 1;
				}
			} else
			if(is_neg)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->u32 = 0;
				return 1;
			}

			if(info->base.flags & KS_TYPEFLAG_HAS_MAX)
			{
				if(val.uval > (kgstruct_uint_t)info->u32.max)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->u32 = info->u32.max;
					return 1;
				}
			}
#ifdef KGSTRUCT_ENABLE_US64
			else
			if(val.uval > 4294967295)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->u32 = 4294967295;
				return 1;
			}
#endif
#endif
			dst->u32 = val.u32;
			return 1;
		case KS_TYPEDEF_S32:
#ifdef KGSTRUCT_ENABLE_MINMAX
			if(info->base.flags & KS_TYPEFLAG_HAS_MIN)
			{
				if(val.sval < (kgstruct_int_t)info->s32.min)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->s32 = info->s32.min;
					return 1;
				}
			}
#ifdef KGSTRUCT_ENABLE_US64
			else
			if(val.sval < -2147483648)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->s32 = -2147483648;
				return 1;
			}
#endif
			if(info->base.flags & KS_TYPEFLAG_HAS_MAX)
			{
				if(val.sval > (kgstruct_int_t)info->s32.max)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->s32 = info->s32.max;
					return 1;
				}
			}
#ifdef KGSTRUCT_ENABLE_US64
			else
			if(val.sval > 2147483647)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->s32 = 2147483647;
				return 1;
			}
#endif
#endif
			dst->s32 = val.s32;
			return 1;
		//// 64 bit
#ifdef KGSTRUCT_ENABLE_US64
		case KS_TYPEDEF_U64:
#ifdef KGSTRUCT_ENABLE_MINMAX
			if(info->base.flags & KS_TYPEFLAG_HAS_MIN)
			{
				if(is_neg || val.uval < (kgstruct_uint_t)info->u64.min)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->u64 = info->u64.min;
					return 1;
				}
			} else
			if(is_neg)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->u64 = 0;
				return 1;
			}

			if(info->base.flags & KS_TYPEFLAG_HAS_MAX)
			{
				if(val.uval > (kgstruct_uint_t)info->u64.max)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->u64 = info->u64.max;
					return 1;
				}
			} else
			if(val.uval > 18446744073709551615UL)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->u64 = 18446744073709551615UL;
				return 1;
			}
#endif
			dst->u64 = val.u64;
			return 1;
		case KS_TYPEDEF_S64:
#ifdef KGSTRUCT_ENABLE_MINMAX
			if(info->base.flags & KS_TYPEFLAG_HAS_MIN)
			{
				if(val.sval < (kgstruct_int_t)info->s64.min)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->s64 = info->s64.min;
					return 1;
				}
			} else
			if(val.sval < -9223372036854775807L) // 9223372036854775808L causes warning
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->s64 = -9223372036854775807L;
				return 1;
			}

			if(info->base.flags & KS_TYPEFLAG_HAS_MAX)
			{
				if(val.sval > (kgstruct_int_t)info->s64.max)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->s64 = info->s64.max;
					return 1;
				}
			} else
			if(val.sval > 9223372036854775807L)
			{
				if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
					return 0;
				dst->s64 = 9223372036854775807L;
				return 1;
			}
#endif
			dst->s64 = val.s64;
			return 1;
#endif
#ifdef KGSTRUCT_ENABLE_FLOAT
		//// float
		case KS_TYPEDEF_FLOAT:
#ifdef KGSTRUCT_ENABLE_MINMAX
			if(info->base.flags & KS_TYPEFLAG_HAS_MAX)
			{
				if(val.f32 > (kgstruct_uint_t)info->f32.max)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->f32 = info->f32.max;
					return 1;
				}
			}
#endif
		dst->f32 = val.f32;
		return 1;
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
		//// double
		case KS_TYPEDEF_DOUBLE:
#ifdef KGSTRUCT_ENABLE_MINMAX
			if(info->base.flags & KS_TYPEFLAG_HAS_MAX)
			{
				if(val.f64 > (kgstruct_uint_t)info->f64.max)
				{
					if(info->base.flags & KS_TYPEFLAG_IGNORE_LIMITED)
						return 0;
					dst->f64 = info->f64.max;
					return 1;
				}
			}
#endif
		dst->f64 = val.f64;
		return 1;
#endif
	}

	return 0;
}

