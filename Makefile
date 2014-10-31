GCC = nspire-gcc
GPP = nspire-g++
LD = nspire-ld
GENZEHN = genzehn
OPTIMIZE ?= fast
GCCFLAGS = -O$(OPTIMIZE) -Wall -W -marm -ffast-math -mcpu=arm926ej-s -fno-math-errno -fomit-frame-pointer -flto -fno-rtti -fgcse-sm -fgcse-las -funsafe-loop-optimizations -fno-fat-lto-objects -frename-registers -fprefetch-loop-arrays -Wno-narrowing
LDFLAGS = -lm -lnspireio
EXE = pyWrite
OBJS = $(patsubst %.c, %.o, $(shell find . -name \*.c))
OBJS += $(patsubst %.cpp, %.o, $(shell find . -name \*.cpp))
OBJS += $(patsubst %.S, %.o, $(shell find . -name \*.S))

all: $(EXE).tns

%.o: %.cpp
	$(GPP) -std=c++11 $(GCCFLAGS) -c $< -o $@

%.o: %.c
	$(GCC) $(GCCFLAGS) -c $< -o $@

%.o: %.S
	$(GCC) $(GCCFLAGS) -c $< -o $@

$(EXE).elf: $(OBJS)
	+$(LD) $^ -o $@ $(GCCFLAGS) $(LDFLAGS)

$(EXE).tns: $(EXE).elf
	+$(GENZEHN) --input $^ --output $@.zehn --name "pyWrite" --version 01 --author "Fabian Vogt"
	+make-prg $@.zehn $@
	+rm $@.zehn

.PHONY: clean
clean:
	rm -f `find . -name \*.o`
	rm -f $(EXE).tns $(EXE).elf
