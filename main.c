#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include "kgstruct.h"
#include "ks_json.h"
#include "structs.h" // generated file

static kgstruct_json_t ks;
static uint8_t buffer[16 * 1024];
static int size;
static int fd;

static test_struct_t test_struct[2];
static test_struct_tf test_fill;

static void dump_struct(test_struct_t *in)
{
	printf("custom_test: 0x%08X\n", in->custom_test);
	printf("flag_test: 0x%08X\n", in->flag_test);

	printf("test_u8: %u\n", in->test_u8);
	printf("test_u16: %u\n", in->test_u16);
	printf("test_u32: %u\n", in->test_u32);
	printf("test_u64: %lu\n", in->test_u64);

	printf("test_s8: %d\n", in->test_s8);
	printf("test_s16: %d\n", in->test_s16);
	printf("test_s32: %d\n", in->test_s32);
	printf("test_s64: %ld\n", in->test_s64);

	printf("test_bool: %d\n", in->test_bool);

	printf("test_float: %f\n", in->test_float);

	printf("test_text: %s\n", in->test_text);

	printf("test_named: %u\n", in->test_named);

	printf("test_limit0: %u\n", in->test_limit0);
	printf("test_limit1: %u\n", in->test_limit1);
	printf("test_limit2: %u\n", in->test_limit2);
	printf("test_limit3: %u\n", in->test_limit3);

	for(uint32_t i = 0; i < 4; i++)
		printf("test_u32a[%u]: %u\n", i, in->test_u32a[i]);

	printf("inside.value_u8: %u\n", in->inside.value_u8);
	printf("inside.value_u16: %u\n", in->inside.value_u16);
	printf("inside.value_u32: %u\n", in->inside.value_u32);
	printf("inside.meta.meta_u32: %u\n", in->inside.meta.meta_u32);
	printf("inside.meta.meta_s32: %d\n", in->inside.meta.meta_s32);

	printf("insidE.value_u8: %u\n", in->insidE.value_u8);
	printf("insidE.value_u16: %u\n", in->insidE.value_u16);
	printf("insidE.value_u32: %u\n", in->insidE.value_u32);
	printf("insidE.meta.meta_u32: %u\n", in->insidE.meta.meta_u32);
	printf("insidE.meta.meta_s32: %d\n", in->insidE.meta.meta_s32);

	for(uint32_t i = 0; i < 4; i++)
	{
		printf("meta_array[%i].meta_u32: %u\n", i, in->meta_array[i].meta_u32);
		printf("meta_array[%i].meta_s32: %d\n", i, in->meta_array[i].meta_s32);
		printf("meta_array[%i].sub[0].test: %d\n", i, in->meta_array[i].sub[0].test);
		printf("meta_array[%i].sub[0].text: %s\n", i, in->meta_array[i].sub[0].text);
		printf("meta_array[%i].sub[1].test: %d\n", i, in->meta_array[i].sub[1].test);
		printf("meta_array[%i].sub[1].text: %s\n", i, in->meta_array[i].sub[1].text);
	}

	printf("[FILL] test_u32a %u\n", test_fill.test_u32a);
	printf("[FILL] meta_array %u;\n", test_fill.meta_array);
	for(uint32_t i = 0; i < 4; i++)
		printf(" meta_array[%u].sub %u\n", i, test_fill.__meta_array[i].sub);
}

uint32_t custom_value_parse(void *dst, const uint8_t *text, uint32_t is_string)
{
	// example of custom parser
	// copy ASCII characters to uint32_t
	uint32_t *number;

	if(!is_string)
		// value was not handled
		return 0;

	number = dst;
	*number = 0;
	strncpy((void*)number, text, sizeof(uint32_t));

	// value was handled
	return 1;
}

uint32_t custom_value_export(void *src, uint8_t *text)
{
	// example of custom exporter
	// copy uint32_t to ASCII characters
	memcpy(text, src, sizeof(uint32_t));
	text[sizeof(uint32_t)] = 0;
	return 1; // it's a string
}

int main(int argc, char **argv)
{
	int ret;
	uint8_t *ptr;

	// BEWARE!
	// This is only a test code, there are no overflow checks.

	if(argc < 2)
	{
		printf("usage: %s test.json\n", argv[0]);
		return 1;
	}

	fd = open(argv[1], O_RDONLY);
	if(fd < 0)
	{
		printf("can't open input file\n");
		return 1;
	}

	size = read(fd, buffer, sizeof(buffer));
	close(fd);

	//
	// IMPORT

	// check full
	printf("== FULL ==========\n");
	ks_json_init(&ks, &ks_template__test_struct, test_struct + 0, &test_fill);
	ret = ks_json_parse(&ks, buffer, size);
	printf("ret: %u\n", ret);

	// check stepped
	printf("\n== STEP ==========\n");
	ks_json_init(&ks, &ks_template__test_struct, test_struct + 1, NULL);
	for(uint32_t i = 0; i < size; i++)
		if(ks_json_parse(&ks, buffer + i, 1) != KS_JSON_MORE_DATA)
			break;

	printf("FULL\n");
	dump_struct(test_struct + 0);
	printf("STEP\n");
	dump_struct(test_struct + 1);

	if(memcmp(test_struct + 0, test_struct + 1, sizeof(test_struct_t)))
		printf("*MISMATCH*\n");
	else
		printf("*MATCH*\n");

	//
	// EXPORT
	memset(buffer, 0, sizeof(buffer));

	// check full
	ks_json_init(&ks, &ks_template__test_struct, test_struct + 0, NULL);
ks.escaped = 1; // readable
	ret = ks_json_export(&ks, buffer, sizeof(buffer) / 2);
	printf("export length: %u\n", ret);

	// check stepped
	ks_json_init(&ks, &ks_template__test_struct, test_struct + 0, NULL);
ks.escaped = 1; // readable
	ptr = buffer + sizeof(buffer) / 2;
	while(1)
	{
		if(!ks_json_export(&ks, ptr, 1))
			break;
		ptr++;
	}

	// print export output
	printf("==== OUTPUT ====\n%s\n================\n", buffer);

	if(strcmp(buffer, buffer + sizeof(buffer) / 2))
		printf("*MISMATCH*\n");
	else
		printf("*MATCH*\n");

	return 0;
}


