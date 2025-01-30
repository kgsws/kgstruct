
// example of external configuration file

// kgStruct
#define KGSTRUCT_FILLINFO_TYPE	uint8_t // enable special 'fill info' structure
#define KGSTRUCT_ENABLE_MINMAX	// enable limits on numbers
#define KGSTRUCT_ENABLE_US64	// enable uint64_t and int64_t
#define KGSTRUCT_ENABLE_FLOAT	// enable usage of 'float'
#define KGSTRUCT_ENABLE_DOUBLE	// enable usage of 'double'
#define KGSTRUCT_ENABLE_CUSTOM_TYPE	// enable custom types
#define KGSTRUCT_ENABLE_FLAGS	// enable boolean flags

// kgStruct: JSON
#define KS_JSON_PARSER
#define KS_JSON_EXPORTER
#define KS_JSON_MAX_STRING_LENGTH	64
#define KS_JSON_MAX_DEPTH	16
//#define KS_JSON_LINE_COUNTER
//#define KS_JSON_ALLOW_STRING_NUMBERS
#define KS_JSON_ENABLE_FILLINFO

// kgStruct: CBOR
#define KS_CBOR_IMPORTER
#define KS_CBOR_EXPORTER
#define KS_CBOR_MAX_KEY_LENGTH	16
#define KS_CBOR_MAX_DEPTH	16
//#define KS_CBOR_BOOL_AS_INTEGER
//#define KS_CBOR_ENABLE_FILLINFO	// NOT IMPLEMENTED

