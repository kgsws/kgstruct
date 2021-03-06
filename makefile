program = test
CFGOBJ =
OBJ = ${CFGOBJ} main.o ks_json.o
LIBS = 
OPT= -O2 -g
CC = gcc
CFLAGS = ${OPT}

build: ${program}

clean:
	rm -f *.o ${program} $(CFGOBJ:.o=.c) $(CFGOBJ:.o=.h)

${program}: ${OBJ}
	${CC} ${OBJ} ${LIBS} -o ${program} ${OPT}

