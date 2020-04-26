TARGET=psh
BUILD=bld
MOD=mod
BASE=src
SUB=pickle httpc cdb shrink
CFLAGS=-Wall -Wextra -std=c99 -O2 -fwrapv -I${BUILD}/include -L${BUILD}/lib -I ${BASE} -L ${BASE} 
AR=ar
ARFLAGS=rcs

.PHONY: all run test modules clean

all: ${TARGET}

test:
	for m in ${SUB}; do\
		make -C "${MOD}/$$m" test;\
	done;

.git:
	git clone https://github.com/howerj/pickle pickle-repo
	mv pickle-repo/.git .
	rm -rf pickle-repo

${MOD}/pickle/.git: .git
	git submodule init
	git submodule update --recursive

modules: ${MOD}/pickle/.git
	mkdir -p ${BUILD}/lib
	mkdir -p ${BUILD}/include
	for m in ${SUB}; do\
		make -C "${MOD}/$$m";\
		cp ${MOD}/$$m/lib$$m.a ${BUILD}/lib;\
		cp ${MOD}/$$m/$$m.h ${BUILD}/include;\
	done;

run: ${TARGET}
	./${TARGET} ${MOD}/pickle/shell

-include ${BASE}/makefile

${TARGET}: modules ${BASE}/psh.o main.o 
	${CC} ${CFLAGS} main.o ${BASE}/psh.o -lpickle -o $@
	-strip ${TARGET}

clean: .git
	for m in ${SUB}; do\
		make -C "${MOD}/$$m" clean;\
	done;
	git clean -dffx
