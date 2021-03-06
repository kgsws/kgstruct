#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include "ks_json.h"

static kgstruct_json_t ks;
static uint8_t buffer[1024];
static int size;
static int fd;

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
	ks_json_reset(&ks);
	ks_json_parse(&ks, buffer, size);

	// check full
	printf("\n== STEP ==========\n");
	ks_json_reset(&ks);
	for(uint32_t i = 0; i < size; i++)
		if(ks_json_parse(&ks, buffer + i, 1) != KS_JSON_MORE_DATA)
			break;

	return 0;
}

