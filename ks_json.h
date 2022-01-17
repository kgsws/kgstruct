
// configuration
#define KS_JSON_MAX_STRING_LENGTH	64
#define KS_JSON_MAX_DEPTH	32
//#define KS_JSON_ALLOW_STRING_NUMBERS

#define KS_JSON_PARSER
#define KS_JSON_EXPORTER

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
	KS_JSON_SYNTAX,
	KS_JSON_BAD_STATE
};

typedef struct
{
	const ks_template_t *template;
	uint32_t offset;
	uint32_t step;
	uint32_t limit;
} kgstruct_json_recursion_t;

typedef struct kgstruct_json_s
{
	uint8_t state;
	uint8_t error;
	uint8_t escaped;
	uint8_t array;
	uint8_t val_type;
	uint8_t depth;
	void *data;
#ifdef KS_JSON_PARSER
	uint8_t str[KS_JSON_MAX_STRING_LENGTH];
#endif
#ifdef KS_JSON_EXPORTER
	uint8_t *(*export_step)(struct kgstruct_json_s*,uint8_t*,uint8_t*);
#endif
	uint8_t *ptr;
	kgstruct_json_recursion_t recursion[KS_JSON_MAX_DEPTH];
	const ks_template_t *element;
} kgstruct_json_t;

#ifdef KS_JSON_PARSER
int ks_json_parse(kgstruct_json_t *ks, const uint8_t *buff, uint32_t length);
#endif
#ifdef KS_JSON_EXPORTER
uint32_t ks_json_export(kgstruct_json_t *ks, uint8_t *buff, uint32_t length);
#endif
void ks_json_init(kgstruct_json_t *ks, void *ptr, const ks_template_t *template);

