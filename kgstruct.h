
// configuration
#define KGSTRUCT_FILLINFO_TYPE	uint8_t // enable special 'fill info' structure
#define KGSTRUCT_ENABLE_MINMAX	// enable limits on numbers
#define KGSTRUCT_ENABLE_US64	// enable uint64_t and int64_t
#define KGSTRUCT_ENABLE_FLOAT	// enable usage of 'float'
#define KGSTRUCT_ENABLE_DOUBLE	// enable usage of 'double'
#define KGSTRUCT_ENABLE_TIME_SPLIT	// enable usage of 'time_split'
#define KGSTRUCT_ENABLE_TIME_MULT	// enable usage of 'time_mult'

//
// internal stuff
#define KGSTRUCT_DECIMAL_ENABLED	(defined(KGSTRUCT_ENABLE_FLOAT) || defined(KGSTRUCT_ENABLE_DOUBLE))
#ifdef KGSTRUCT_ENABLE_US64
#define KGSTRUCT_NUMBER_SMAX	0x7FFFFFFFFFFFFFFFL
#else
#define KGSTRUCT_NUMBER_SMAX	0x7FFFFFFF
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
#ifdef KGSTRUCT_ENABLE_FLOAT
	KS_TYPEDEF_FLOAT,
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
	KS_TYPEDEF_DOUBLE,
#endif
#ifdef KGSTRUCT_ENABLE_TIME_SPLIT
	KS_TYPEDEF_TIME_SPLIT,
#endif
#ifdef KGSTRUCT_ENABLE_TIME_MULT
	KS_TYPEDEF_TIME_MULT,
#endif
	// non-value types (strings and stuff)
	KS_TYPEDEF_STRING,
	KS_TYPEDEF_STRUCT,
};
#define KS_TYPE_LAST_NUMERIC	KS_TYPEDEF_STRING

// flags
#define KS_TYPEFLAG_HAS_MIN	1
#define KS_TYPEFLAG_HAS_MAX	2
#define KS_TYPEFLAG_IGNORE_LIMITED	4
#define KS_TYPEFLAG_EMPTY_ARRAY	8

#define KS_TYPEFLAG_HAS_SECONDS	1

//
// secial types

// time, split into elements
typedef struct
{
	uint8_t h, m, s;
	uint8_t unused;
} kgstruct_time_t;

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
	uint64_t uread;
	int64_t sread;
#else
	uint32_t uread;
	int32_t sread;
#endif
#ifdef KGSTRUCT_ENABLE_FLOAT
	float f32;
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
	double f64;
#endif
#ifdef KGSTRUCT_ENABLE_TIME_SPLIT
	kgstruct_time_t time;
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

typedef struct
{
	kgstruct_base_t base;
} kgstruct_string_t;

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
	kgstruct_string_t string;
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
	uint8_t *key;
	kgstruct_type_t *info;
	uint32_t offset;
#ifdef KGSTRUCT_FILLINFO_TYPE
	uint32_t fill_offs;
#endif
} ks_template_t;

typedef struct ks_base_template_s
{
#ifdef KGSTRUCT_FILLINFO_TYPE
	uint32_t fill_size;
#endif
	ks_template_t template[];
} ks_base_template_t;

