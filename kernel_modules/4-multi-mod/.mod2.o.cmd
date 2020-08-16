cmd_/home/macaron/linux/tools/labs/skels/./kernel_modules/4-multi-mod/mod2.o := gcc -Wp,-MD,/home/macaron/linux/tools/labs/skels/./kernel_modules/4-multi-mod/.mod2.o.d  -nostdinc -isystem /usr/lib/gcc/x86_64-linux-gnu/7/include -I./arch/x86/include -I./arch/x86/include/generated  -I./include -I./arch/x86/include/uapi -I./arch/x86/include/generated/uapi -I./include/uapi -I./include/generated/uapi -include ./include/linux/kconfig.h -include ./include/linux/compiler_types.h -D__KERNEL__ -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -Werror-implicit-function-declaration -Wno-format-security -std=gnu89 -fno-PIE -DCC_HAVE_ASM_GOTO -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx -m32 -msoft-float -mregparm=3 -freg-struct-return -fno-pic -mpreferred-stack-boundary=2 -march=i686 -Wa,-mtune=generic32 -ffreestanding -DCONFIG_AS_CFI=1 -DCONFIG_AS_CFI_SIGNAL_FRAME=1 -DCONFIG_AS_CFI_SECTIONS=1 -DCONFIG_AS_SSSE3=1 -DCONFIG_AS_CRC32=1 -DCONFIG_AS_AVX=1 -DCONFIG_AS_AVX2=1 -DCONFIG_AS_AVX512=1 -DCONFIG_AS_SHA1_NI=1 -DCONFIG_AS_SHA256_NI=1 -pipe -Wno-sign-compare -fno-asynchronous-unwind-tables -mindirect-branch=thunk-extern -mindirect-branch-register -DRETPOLINE -fno-delete-null-pointer-checks -Wno-frame-address -Wno-format-truncation -Wno-format-overflow -Wno-int-in-bool-context -O2 --param=allow-store-data-races=0 -Wframe-larger-than=1024 -fstack-protector-strong -Wno-unused-but-set-variable -Wno-unused-const-variable -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-var-tracking-assignments -g -gdwarf-4 -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fno-merge-all-constants -fmerge-constants -fno-stack-check -fconserve-stack -Werror=implicit-int -Werror=strict-prototypes -Werror=date-time -Werror=incompatible-pointer-types -Werror=designated-init -Wno-unused-function -Wno-unused-label -Wno-unused-variable  -DMODULE  -DKBUILD_BASENAME='"mod2"' -DKBUILD_MODNAME='"multi_mod"' -c -o /home/macaron/linux/tools/labs/skels/./kernel_modules/4-multi-mod/mod2.o /home/macaron/linux/tools/labs/skels/./kernel_modules/4-multi-mod/mod2.c

source_/home/macaron/linux/tools/labs/skels/./kernel_modules/4-multi-mod/mod2.o := /home/macaron/linux/tools/labs/skels/./kernel_modules/4-multi-mod/mod2.c

deps_/home/macaron/linux/tools/labs/skels/./kernel_modules/4-multi-mod/mod2.o := \
  include/linux/kconfig.h \
    $(wildcard include/config/cpu/big/endian.h) \
    $(wildcard include/config/booger.h) \
    $(wildcard include/config/foo.h) \
  include/linux/compiler_types.h \
    $(wildcard include/config/have/arch/compiler/h.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/arch/supports/optimized/inlining.h) \
    $(wildcard include/config/optimize/inlining.h) \
  include/linux/compiler-gcc.h \
    $(wildcard include/config/arch/use/builtin/bswap.h) \

/home/macaron/linux/tools/labs/skels/./kernel_modules/4-multi-mod/mod2.o: $(deps_/home/macaron/linux/tools/labs/skels/./kernel_modules/4-multi-mod/mod2.o)

$(deps_/home/macaron/linux/tools/labs/skels/./kernel_modules/4-multi-mod/mod2.o):
