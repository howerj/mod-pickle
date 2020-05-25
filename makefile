TARGET=pickle
BUILD=bld
MOD=mod
BASE=src
SUB=pickle httpc cdb shrink utf8
CFLAGS=-Wall -Wextra -std=c99 -g -O2 -fwrapv -I${MOD} -I${BUILD}/include -L${BUILD}/lib -I ${BASE} -L ${BASE} 
AR=ar
ARFLAGS=rcs
LDLIBS=-lpickle -lmod -lcdb -lhttpc -lutf8
USE_SSL=1
#LDLIBS=${SUB:%=-l%}
#LDLIBS+=-lmod

.PHONY: all run test modules clean tags

ifeq ($(OS),Windows_NT)
EXE=.exe
DLL=dll
PLATFORM=win
LDLIBS += -lWs2_32
STRIP=rem
else # Assume Unixen
EXE=
DLL=so
PLATFORM=unix
STRIP=strip
#STRIP=\#
endif

ifeq ($(USE_SSL),1)
ifeq ($(PLATFORM),unix)
LDLIBS += -lssl
else
endif
endif

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

run: ${TARGET}${EXE}
	./${TARGET}${EXE} ${MOD}/pickle/shell

-include ${BASE}/makefile.in

${TARGET}${EXE}: modules ${BASE}/libmod.a main.o 
	${CC} ${CFLAGS} main.o ${LDLIBS} -o $@
	-${STRIP} ${TARGET}

clean: .git
	for m in ${SUB}; do\
		make -C "${MOD}/$$m" clean;\
	done;
	git clean -dffx

tags:
	ctags -R -f ./.git/tags .

