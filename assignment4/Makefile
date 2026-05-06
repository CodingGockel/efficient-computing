#https://spin.atomicobject.com/2016/08/26/makefile-c-projects/
#https://github.com/johnwinans/riscv-toolchain-install-guide/

#BIG CORE
# ~/Documents/k230_sdk/toolchain/riscv64-linux-musleabi_for_x86_64-pc-linux-gnu/bin/riscv64-unknown-linux-musl-gcc
#SMALL CORE
# ~/Documents/k230_sdk/toolchain/Xuantie-900-gcc-linux-5.10.4-glibc-x86_64-V2.6.0/bin/riscv64-unknown-linux-gnu-gcc

#ARCH=~/Documents/k230_sdk/toolchain/riscv64-linux-musleabi_for_x86_64-pc-linux-gnu/bin/riscv64-unknown-linux-musl-

ARCH=
#MARCH=rv64imafdcv
#MABI=lp64d 

CC=$(ARCH)gcc
CXX=$(ARCH)g++
AS=$(ARCH)gcc
LD=$(ARCH)ld
OBJCOPY=$(ARCH)objcopy
OBJDUMP=$(ARCH)objdump
SIZE=$(ARCH)size
AR=$(ARCH)ar
RANLIB=$(ARCH)ranlib

#od does not exist for K230
#OD=$(ARCH)-od
OD=od

#TARGET_ARCH+=-march=$(MARCH)
#TARGET_ARCH+=-mabi=$(MABI)
TARGET_ARCH+=-mcpu=cortex-a72 

OPTIMIZE = -O0
DEBUG = -g

CFLAGS = $(OPTIMIZE) $(DEBUG) $(TARGET_ARCH) -c #-malign-data=natural -mpreferred-stack-boundary=4 -mno-strict-align -mtune=sifive-7-series
CXXFLAGS = -c  $(TARGET_ARCH) $(OPTIMIZE) $(DEBUG) 
ASFLAGS = -Wall #-march=$(MARCH) 

CFLAGS+=-Wall -Wextra -Wpedantic #-Werror
CFLAGS+=-Wcast-align
#CFLAGS+=-ffreestanding 
#CFLAGS+=-fno-pic
#CFLAGS+=-fpic
#CFLAGS+=-fomit-frame-pointer
CXXFLAGS+=$(CFLAGS)

LDLIBS=-lc -lgcc -lm
LDFLAGS=
#LDFLAGS+=-nostdlib
#LDFLAGS+=-Wl,-T,vanilla.ld

#ASFLAGS+=$(CFLAGS)
#ASFLAGS+=-Wa,-alh=$@.lst,-L

OBJDUMPFLAGS=

TARGET_EXEC ?= exec.out

BUILD_DIR ?= ./build
SRC_DIRS ?= ./src

SRCS := $(shell find $(SRC_DIRS) -name *.cpp -or -name *.c -or -name *.s)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
CPPFLAGS ?= $(INC_FLAGS) -MMD -MP 

CSOURCES=$(shell find $(SRC_DIRS) -name *.c -or -name *.cpp)
ASSEMBLY = $(CSOURCES:%=$(BUILD_DIR)/%.asm)
COBJECTS = $(CSOURCES:%=$(BUILD_DIR)/%.o)
DISASMC = $(CSOURCES:%=$(BUILD_DIR)/%.dis)

ASMSOURCES := $(shell find $(SRC_DIRS) -name *.s)
ASMOBJECTS = $(ASMSOURCES:%=$(BUILD_DIR)/%.o)
HEX = $(ASMSOURCES:%=$(BUILD_DIR)/%.hex)
DISASM = $(ASMSOURCES:%=$(BUILD_DIR)/%.dis)

HEX_CLEAN = $(CSOURCES:%=$(BUILD_DIR)/%.hexc)

all: OBJDUMPFLAGS+=-Mnumeric,no-aliases
all: OBJDUMPFLAGS+=-S -d
all: $(BUILD_DIR)/$(TARGET_EXEC) $(HEX) $(ASSEMBLY)

#the following is for the AD-Processor
link_no_std: LDFLAGS+=-nostdlib
link_no_std: LDFLAGS+=-Wl,-T,vanilla.ld 
link_no_std: OBJDUMPFLAGS+=-d -j .text -j .plt
link_no_std: $(BUILD_DIR)/$(TARGET_EXEC) $(HEX) $(ASSEMBLY) $(HEX_CLEAN_ASM) $(HEX_CLEAN_C)

#the following is for the AD-Processor
link_std: LDFLAGS+=-Wl,-T,vanilla_std.ld,--entry=0x00000000
link_std: OBJDUMPFLAGS+=-d -j .text -j .plt
link_std: $(BUILD_DIR)/$(TARGET_EXEC) $(HEX) $(ASSEMBLY) $(HEX_CLEAN_ASM) $(HEX_CLEAN_C)

$(ASSEMBLY):
	#directly from C-code files compiled assembly
	for i in $(CSOURCES); do\
		$(CC) -S $$i -o $(BUILD_DIR)/$$i.asm;\
	done 
	#from C-Obj files disassembly
	for i in $(COBJECTS); do\
		$(OBJDUMP) --syms -S -d $$i > $$i.dis;\
		$(OD) -A x4 -t x1 $(ASMOBJECTS) > $(HEX);\
	done
$(HEX):
	for i in $(ASMOBJECTS); do\
		$(OBJDUMP) --syms -S -d $$i > $$i.dis;\
		$(OD) -A x4 -t x1 $$i > $$i.hex;\
	done
$(HEX_CLEAN):
	# Author: Stefan Remke
	for i in $(CSOURCES); do\
		hexdump -ve '1/1 " %.8x,"' exec.out;\
	done
$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)
	$(OBJDUMP) $(OBJDUMPFLAGS) $(BUILD_DIR)/$(TARGET_EXEC) >  $(BUILD_DIR)/$(TARGET_EXEC).dis
	$(OBJDUMP) -s -EB -j .data -j .rodata -j .text $(BUILD_DIR)/$(TARGET_EXEC) >  $(BUILD_DIR)/$(TARGET_EXEC).full #objdumps section .text .rodata AND .data to exec.out.full
	$(OBJDUMP) $(OBJDUMPFLAGS) -dr $< > $<.lst
	$(SIZE) -x -A $@
# assembly
$(BUILD_DIR)/%.s.o: %.s
	$(MKDIR_P) $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@
# c source
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
# c++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
