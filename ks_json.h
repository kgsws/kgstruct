
// configuration
#define KS_JSON_MAX_STRING_LENGTH	32
#define KS_JSON_MAX_DEPTH	8

//
//

enum
{
	// common returns
	KS_JSON_OK,
	KS_JSON_MORE_DATA,
	KS_JSON_TOO_DEEP,
	KS_JSON_TOO_LONG_STRING,
	// JSON specific
	KS_JSON_EXPECTED_OBJECT,
	KS_JSON_EXPECTED_KEY,
	KS_JSON_EXPECTED_KEY_SEPARATOR,
	KS_JSON_SYNTAX
};

typedef struct
{
	uint8_t state;
	uint8_t error;
	uint8_t escaped;
	uint8_t array;
	uint8_t val_type;
	uint8_t depth;
	uint8_t str[KS_JSON_MAX_STRING_LENGTH];
	uint8_t *ptr;
	uint32_t dbit[(KS_JSON_MAX_DEPTH+31) >> 5];
} kgstruct_json_t;

int ks_json_parse(kgstruct_json_t *ks, uint8_t *buff, uint32_t length);
void ks_json_reset(kgstruct_json_t *ks);

