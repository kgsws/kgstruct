program = test
KSOBJ = structs.o
OBJ = main.o ks_json.o
LIBS = 
OPT= -O2 -g
CC = gcc
CFLAGS = ${OPT}

build: ${KSOBJ} ${program}

structs.c: structs.json
	./ks_generator.py -code structs.json structs

clean:
	rm -f *.o ${program} $(KSOBJ:.o=.c) $(KSOBJ:.o=.h)

${program}: ${OBJ}
	${CC} ${OBJ} ${KSOBJ} ${LIBS} -o ${program} ${OPT}

