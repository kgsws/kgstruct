

// configuration
#ifndef KGSTRUCT_EXTERNAL_CONFIG

#define KGSTRUCT_FILLINFO_TYPE	uint8_t // enable special 'fill info' structure; NOTE: individual parser has it's own enable too
#define KGSTRUCT_ENABLE_MINMAX	// enable limits on numbers
#define KGSTRUCT_ENABLE_US64	// enable uint64_t and int64_t
#define KGSTRUCT_ENABLE_FLOAT	// enable usage of 'float'
#define KGSTRUCT_ENABLE_DOUBLE	// enable usage of 'double'
#define KGSTRUCT_ENABLE_CUSTOM_TYPE	// enable custom types
#define KGSTRUCT_ENABLE_FLAGS	// enable boolean flags

#endif

//
// internal stuff
#define KGSTRUCT_DECIMAL_ENABLED	(defined(KGSTRUCT_ENABLE_FLOAT) || defined(KGSTRUCT_ENABLE_DOUBLE))
#ifdef KGSTRUCT_ENABLE_US64
#define KGSTRUCT_NUMBER_SMAX	0x7FFFFFFFFFFFFFFFL
typedef uint64_t kgstruct_uint_t;
typedef int64_t kgstruct_int_t;
#else
#define KGSTRUCT_NUMBER_SMAX	0x7FFFFFFF
typedef uint32_t kgstruct_uint_t;
typedef int32_t kgstruct_int_t;
#endif

// types
enum
{
	KS_TYPEDEF_U8,
	KS_TYPEDEF_S8,
	KS_TYPEDEF_U16,
	KS_TYPEDEF_S16,
	KS_TYPEDEF_U32,
	KS_TYPEDEF_S32,
#ifdef KGSTRUCT_ENABLE_US64
	KS_TYPEDEF_U64,
	KS_TYPEDEF_S64,
#endif
	//// floating point types
#ifdef KGSTRUCT_ENABLE_FLOAT
	KS_TYPEDEF_FLOAT,
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
	KS_TYPEDEF_DOUBLE,
#endif
	//// non-value types

	KS_TYPEDEF_STRING,
	KS_TYPEDEF_BINARY,

#ifdef KGSTRUCT_ENABLE_CUSTOM_TYPE
	KS_TYPEDEF_CUSTOM,
#endif
	KS_TYPEDEF_STRUCT,

	//// flags
#ifdef KGSTRUCT_ENABLE_FLAGS
	KS_TYPEDEF_FLAGS, // this is like 'KS_TYPEDEF_STRUCT'
	KS_TYPEDEF_FLAG8,
	KS_TYPEDEF_FLAG16,
	KS_TYPEDEF_FLAG32,
#ifdef KGSTRUCT_ENABLE_US64
	KS_TYPEDEF_FLAG64,
#endif
#endif
};

#define KS_TYPE_LAST_NUMERIC	KS_TYPEDEF_STRING

#ifdef KGSTRUCT_ENABLE_US64
#define KS_TYPE_LAST_INTEGER	KS_TYPEDEF_S64+1
#else
#define KS_TYPE_LAST_INTEGER	KS_TYPEDEF_S32+1
#endif

// flags
#define KS_TYPEFLAG_HAS_MIN	1
#define KS_TYPEFLAG_HAS_MAX	2
#define KS_TYPEFLAG_IGNORE_LIMITED	4
#define KS_TYPEFLAG_EMPTY_ARRAY	8
#define KS_TYPEFLAG_IS_BOOL	16

//
// pointer access
typedef union
{
	uint8_t u8;
	int8_t s8;
	uint16_t u16;
	int16_t s16;
	uint32_t u32;
	int32_t s32;
#ifdef KGSTRUCT_ENABLE_US64
	uint64_t u64;
	int64_t s64;
	uint64_t uval;
	int64_t sval;
#else
	uint32_t uval;
	int32_t sval;
#endif
#ifdef KGSTRUCT_ENABLE_FLOAT
	float f32;
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
	double f64;
#endif
#if defined(KGSTRUCT_ENABLE_US64) || defined(KGSTRUCT_ENABLE_DOUBLE)
	uint8_t raw[8];
#else
	uint8_t raw[4];
#endif
} kgstruct_number_t;

//
// type defs

typedef struct
{
	uint16_t type;
	uint16_t flags;
	uint32_t array;
	uint32_t size;
} kgstruct_base_t;

typedef struct
{
	kgstruct_base_t base;
} kgstruct_base_only_t;

#ifdef KGSTRUCT_ENABLE_CUSTOM_TYPE
typedef struct
{
	kgstruct_base_t base;
	uint32_t (*parse)(void *dst, uint8_t *text, uint32_t is_string);
	uint32_t (*exprt)(void *src, uint8_t *text);
} kgstruct_custom_t;
#endif

typedef struct
{
	kgstruct_base_t base;
} kgstruct_string_t;

typedef struct
{
	kgstruct_base_t base;
} kgstruct_binary_t;

typedef struct
{
	kgstruct_base_t base;
	const struct ks_base_template_s *basetemp;
} kgstruct_object_t;

typedef struct
{
	kgstruct_base_t base;
#ifdef KGSTRUCT_ENABLE_MINMAX
	int8_t min, max;
#endif
} kgstruct_s8_t;

typedef struct
{
	kgstruct_base_t base;
#ifdef KGSTRUCT_ENABLE_MINMAX
	uint8_t min, max;
#endif
} kgstruct_u8_t;

typedef struct
{
	kgstruct_base_t base;
#ifdef KGSTRUCT_ENABLE_MINMAX
	int16_t min, max;
#endif
} kgstruct_s16_t;

typedef struct
{
	kgstruct_base_t base;
#ifdef KGSTRUCT_ENABLE_MINMAX
	uint16_t min, max;
#endif
} kgstruct_u16_t;

typedef struct
{
	kgstruct_base_t base;
#ifdef KGSTRUCT_ENABLE_MINMAX
	int32_t min, max;
#endif
} kgstruct_s32_t;

typedef struct
{
	kgstruct_base_t base;
#ifdef KGSTRUCT_ENABLE_MINMAX
	uint32_t min, max;
#endif
} kgstruct_u32_t;

#ifdef KGSTRUCT_ENABLE_US64
typedef struct
{
	kgstruct_base_t base;
#ifdef KGSTRUCT_ENABLE_MINMAX
	int64_t min, max;
#endif
} kgstruct_s64_t;

typedef struct
{
	kgstruct_base_t base;
#ifdef KGSTRUCT_ENABLE_MINMAX
	uint64_t min, max;
#endif
} kgstruct_u64_t;
#endif

#ifdef KGSTRUCT_ENABLE_FLOAT
typedef struct
{
	kgstruct_base_t base;
#ifdef KGSTRUCT_ENABLE_MINMAX
	float min, max;
#endif
} kgstruct_float_t;
#endif

#ifdef KGSTRUCT_ENABLE_DOUBLE
typedef struct
{
	kgstruct_base_t base;
#ifdef KGSTRUCT_ENABLE_MINMAX
	double min, max;
#endif
} kgstruct_double_t;
#endif

typedef union
{
	kgstruct_base_t base;
	kgstruct_base_only_t base_only;
#ifdef KGSTRUCT_ENABLE_CUSTOM_TYPE
	kgstruct_custom_t custom;
#endif
	kgstruct_string_t string;
	kgstruct_binary_t binary;
	kgstruct_object_t object;
	kgstruct_s8_t s8;
	kgstruct_u8_t u8;
	kgstruct_s16_t s16;
	kgstruct_u16_t u16;
	kgstruct_s32_t s32;
	kgstruct_u32_t u32;
#ifdef KGSTRUCT_ENABLE_US64
	kgstruct_s64_t s64;
	kgstruct_u64_t u64;
#endif
#ifdef KGSTRUCT_ENABLE_FLOAT
	kgstruct_float_t f32;
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
	kgstruct_double_t f64;
#endif
} kgstruct_type_t;

//
// template

typedef struct ks_template_s
{
	const uint8_t *key;
	uint32_t kln;
	const kgstruct_type_t *info;
	uint32_t offset;
#if defined(KGSTRUCT_FILLINFO_TYPE) || defined(KGSTRUCT_ENABLE_FLAGS)
	union
	{
		uint32_t fill_offs;
#if defined(KGSTRUCT_ENABLE_US64) && defined(KGSTRUCT_ENABLE_FLAGS)
		uint64_t flag_bits;
#else
		uint32_t flag_bits;
#endif
	};
#endif
} ks_template_t;

typedef struct ks_base_template_s
{
	uint32_t count;
#ifdef KGSTRUCT_FILLINFO_TYPE
	uint32_t fill_size;
#endif
	ks_template_t tmpl[];
} ks_base_template_t;

//

int kgstruct_handle_value(kgstruct_number_t *dst, kgstruct_number_t val, const kgstruct_type_t *info, uint32_t is_neg);
