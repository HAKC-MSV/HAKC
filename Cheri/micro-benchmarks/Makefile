CC=clang
MORELLO_CLANG=/morello/llvm/bin/clang
ELF_PATCH=morello_elf
MUSL_HOME?=/morello/musl
CLANG_RESOURCE_DIR=$(shell clang -print-resource-dir)

BUILD_FLAGS=-I/morello/usr/include --sysroot $(MUSL_HOME)
LINK_FLAGS=-fuse-ld=lld --sysroot $(MUSL_HOME) -rtlib=compiler-rt

PURECAP_TARGET=aarch64-linux-musl_purecap

OUT=./bin

out_dir:
	mkdir -p $(OUT)

clean:
	rm -f $(OUT)/*

purecap: purecap_32 purecap_128

native: native_32 native_128

native_32: out_dir
	$(CC) -g -DBUFFER_SIZE=32 compartment-transfer.c sha256-neon.S -o $(OUT)/compartment-transfer.native.32

native_128: out_dir
	$(CC) -g -DBUFFER_SIZE=128 compartment-transfer.c sha256-neon.S -o $(OUT)/compartment-transfer.native.128

purecap_32: out_dir
	$(MORELLO_CLANG) -c -g -march=morello --target=$(PURECAP_TARGET) $(BUILD_FLAGS) -DCHERI_TEST -DBUFFER_SIZE=32 \
	compartment-transfer.c -o $(OUT)/compartment-transfer.c.purecap.32.o
	$(MORELLO_CLANG) -march=morello --target=$(PURECAP_TARGET) $(LINK_FLAGS) $(OUT)/compartment-transfer.c.purecap.32.o -o \
	$(OUT)/compartment-transfer.purecap.32 -static

purecap_128: out_dir
	$(MORELLO_CLANG) -c -g -march=morello --target=$(PURECAP_TARGET) $(BUILD_FLAGS) -DCHERI_TEST -DBUFFER_SIZE=128 \
	compartment-transfer.c -o $(OUT)/compartment-transfer.c.purecap.128.o
	$(MORELLO_CLANG) -march=morello --target=$(PURECAP_TARGET) $(LINK_FLAGS) $(OUT)/compartment-transfer.c.purecap.128.o -o \
	$(OUT)/compartment-transfer.purecap.128 -static
