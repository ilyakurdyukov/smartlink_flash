## Payload code source

Tested ONLY on SL6801.

### Usage

An example of how to read the ROM:
```
sudo ./smtlink_dump init \
	simple_exec 0x820000 payload/payload.bin \
	read_mem 0 0x5c000 dump.bin
```

### Build

#### with GCC from the old NDK

* GCC has been removed since r18, and hasn't updated since r13. But sometimes it makes the smallest code.

```
NDK=$HOME/android-ndk-r15c
SYSROOT=$NDK/platforms/android-21/arch-arm
TOOLCHAIN=$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi

make all TOOLCHAIN="$TOOLCHAIN" SYSROOT="$SYSROOT"
```

#### with Clang from the old NDK

* NDK, SYSROOT, TOOLCHAIN as before.

```
CLANG="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/clang -target armv7-none-linux-androideabi -gcc-toolchain $NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64"

make all TOOLCHAIN="$TOOLCHAIN" SYSROOT="$SYSROOT" CC="$CLANG"
```

#### with Clang from the newer NDK

```
NDK=$HOME/android-ndk-r25b
TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm
CLANG=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi21-clang

make all TOOLCHAIN=$TOOLCHAIN CC=$CLANG
```

