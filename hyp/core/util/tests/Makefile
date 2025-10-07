# These host tests are compiled to run on AArch64 emulation with
# qemu-system-arm using the host clang and qemu-user-binfmt. The following host
# packages are required: clang libc6-dev-arm64-cross libgcc-10-dev-arm64-cross
# binutils-aarch64-linux-gnu qemu-user-binfmt.
CC=clang -target aarch64-linux-gnu

CFLAGS=-std=c11 --sysroot=/usr/aarch64-linux-gnu -O3 -flto -Wno-gcc-compat
CPPFLAGS=-D_DEFAULT_SOURCE -D__STDC_WANT_LIB_EXT1__=1 -DHYP_STANDALONE_TEST \
	 -I../../../interfaces/util/include \
	 -I../../../arch/aarch64/include \
	 -I../../../arch/generic/include \
	 -Iqemu/include

SRC=../tests/string.c \
    ../aarch64/src/string.c \
    ../src/string_util.c \
    ../aarch64/src/memset.S \
    ../aarch64/src/memcpy.S

LDFLAGS=-static

OBJ=string-test

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ -o $@
