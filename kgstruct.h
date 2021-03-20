
// configuration
#define KGSTRUCT_MAX_64K	// structures are no bigger than 64k
#define KGSTRUCT_ENABLE_MINAX	// enable limits on numbers
#define KGSTRUCT_ENABLE_US64	// enable uint64_t and int64_t
#define KGSTRUCT_ENABLE_FLOAT	// enable usage of 'float'
#define KGSTRUCT_ENABLE_DOUBLE	// enable usage of 'double'

//
// internal stuff
#define KGSTRUCT_DECIMAL_ENABLED	(defined(KGSTRUCT_ENABLE_FLOAT) || defined(KGSTRUCT_ENABLE_DOUBLE))
#ifdef KGSTRUCT_ENABLE_US64
#define KGSTRUCT_NUMBER_SMAX	0x7FFFFFFFFFFFFFFFL
#else
#define KGSTRUCT_NUMBER_SMAX	0x7FFFFFFF
#endif

enum
{
	KS_TYPEDEF_STRING,
	KS_TYPEDEF_U8,
	KS_TYPEDEF_U16,
	KS_TYPEDEF_U32,
#ifdef KGSTRUCT_ENABLE_US64
	KS_TYPEDEF_U64,
#endif
	KS_TYPEDEF_S8,
	KS_TYPEDEF_S16,
	KS_TYPEDEF_S32,
#ifdef KGSTRUCT_ENABLE_US64
	KS_TYPEDEF_S64,
#endif
#ifdef KGSTRUCT_ENABLE_FLOAT
	KS_TYPEDEF_FLOAT,
#endif
#ifdef KGSTRUCT_ENABLE_DOUBLE
	KS_TYPEDEF_DOUBLE,
#endif
	// flags
	KS_TYPEMASK = 0x000000FF,
#ifdef KGSTRUCT_ENABLE_MINAX
	KS_TYPEFLAG_HAS_MIN = 0x80000000,
	KS_TYPEFLAG_HAS_MAX = 0x40000000,
#endif
};

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
} kgstruct_number_t;

//
// type defs

typedef struct
{
	uint32_t type;
} kgstruct_base_t;

typedef struct
{
	kgstruct_base_t base;
} kgstruct_base_base_t;

typedef struct
{
	kgstruct_base_t base;
	uint32_t extra[1];
} kgstruct_string_t;

typedef struct
{
	kgstruct_base_t base;
	int8_t extra[2];
} kgstruct_s8_t;

typedef struct
{
	kgstruct_base_t base;
	uint8_t extra[2];
} kgstruct_u8_t;

typedef struct
{
	kgstruct_base_t base;
	int16_t extra[2];
} kgstruct_s16_t;

typedef struct
{
	kgstruct_base_t base;
	uint16_t extra[2];
} kgstruct_u16_t;

typedef struct
{
	kgstruct_base_t base;
	int32_t extra[2];
} kgstruct_s32_t;

typedef struct
{
	kgstruct_base_t base;
	uint32_t extra[2];
} kgstruct_u32_t;

#ifdef KGSTRUCT_ENABLE_US64
typedef struct
{
	kgstruct_base_t base;
	int64_t extra[2];
} kgstruct_s64_t;

typedef struct
{
	kgstruct_base_t base;
	uint64_t extra[2];
} kgstruct_u64_t;
#endif

#ifdef KGSTRUCT_ENABLE_FLOAT
typedef struct
{
	kgstruct_base_t base;
	float extra[2];
} kgstruct_float_t;
#endif

#ifdef KGSTRUCT_ENABLE_DOUBLE
typedef struct
{
	kgstruct_base_t base;
	double extra[2];
} kgstruct_double_t;
#endif

typedef union
{
	kgstruct_base_t base;
	kgstruct_string_t string;
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

typedef struct
{
	const uint8_t *key;
	const kgstruct_type_t *info;
#ifdef KGSTRUCT_MAX_64K
	uint16_t offset;
#else
	uint32_t offset;
#endif
	uint16_t skip;
} kgstruct_template_t;

