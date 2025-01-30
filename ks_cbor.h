
// configuration
#ifndef KGSTRUCT_EXTERNAL_CONFIG

#define KS_CBOR_IMPORTER
#define KS_CBOR_EXPORTER
#define KS_CBOR_MAX_KEY_LENGTH	16
#define KS_CBOR_MAX_DEPTH	16
//#define KS_CBOR_BOOL_AS_INTEGER
//#define KS_CBOR_ENABLE_FILLINFO	// NOT IMPLEMENTED

#endif

//
//

enum
{
	// common returns
	KS_CBOR_OK,
	KS_CBOR_MORE_DATA,
	KS_CBOR_TOO_DEEP,
	KS_CBOR_TOO_LONG_STRING,
	// CBOR specific
	KS_CBOR_NEED_OBJECT,
	KS_CBOR_NEED_ARRAY,
	KS_CBOR_UNSUPPORTED,
	KS_CBOR_BAD_KEY_TYPE,
};

struct kgstruct_cbor_s;

typedef struct
{
	union
	{
		const ks_template_t *tmpl; // importer
		const ks_base_template_t *base; // exporter
	};
	const ks_template_t *ktpl;
	int (*base_func)(struct kgstruct_cbor_s *ks);
	void *buff;
	uint32_t count;
	uint32_t idx;
	uint32_t asize;
	uint32_t amax;
} kgstruct_cbor_recursion_t;

typedef struct kgstruct_cbor_s
{
	uint8_t *buff;
	uint8_t *end;
	uint8_t *ptr;
#ifdef KS_CBOR_EXPORTER
	uint8_t *top;
#endif
	uint32_t depth;
	uint32_t val_cnt;
	uint32_t byte_cnt;
	uint32_t skip_cnt;
	uint32_t key_len;
	uint32_t val_type;
	kgstruct_number_t value;
	int (*step)(struct kgstruct_cbor_s *ks);
#ifdef KS_CBOR_MAX_KEY_LENGTH
	uint8_t key[KS_CBOR_MAX_KEY_LENGTH];
#endif
	void *next_func;
	kgstruct_cbor_recursion_t recursion[KS_CBOR_MAX_DEPTH];
} kgstruct_cbor_t;

#ifdef KS_CBOR_IMPORTER
int ks_cbor_feed(kgstruct_cbor_t *ks, const uint8_t *buff, uint32_t length);
#endif
#ifdef KS_CBOR_EXPORTER
uint32_t ks_cbor_export(kgstruct_cbor_t *ks, uint8_t *buff, uint32_t length);
#endif
#ifdef KS_CBOR_ENABLE_FILLINFO
#error CBOR fill-info is not implemented!
#else
#ifdef KS_CBOR_IMPORTER
void ks_cbor_init_import(kgstruct_cbor_t *ks, const ks_base_template_t *basetemp, void *buffer);
#endif
#ifdef KS_CBOR_EXPORTER
void ks_cbor_init_export(kgstruct_cbor_t *ks, const ks_base_template_t *basetemp, void *buffer);
#endif
#endif
