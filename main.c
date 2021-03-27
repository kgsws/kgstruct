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
static uint8_t buffer[2048];
static int size;
static int fd;

static test_struct_t test_struct[2];

static void dump_struct(test_struct_t *in)
{
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
}

int main(int argc, char **argv)
{
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

	// check full
	printf("== FULL ==========\n");
	ks_json_init(&ks, test_struct + 0, ks_template__test_struct);
	ks_json_parse(&ks, buffer, size);
/*
	// check stepped
	printf("\n== STEP ==========\n");
	ks_json_init(&ks, test_struct + 1, ks_template__test_struct);
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
*/
	return 0;
}

