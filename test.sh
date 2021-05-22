#!/bin/bash

cd ../output-clang-i386 && /usr/bin/clang-12 --target=i686-w64-mingw32 --sysroot=/usr/RosBE/i386 -DDBG=1 -DKDBG -DMINGW_HAS_SECURE_API=1 -DNTDDI_VERSION=0x05020400 -DUSE_COMPILER_EXCEPTIONS -DWINVER=0x502 -D_GLIBCXX_HAVE_BROKEN_VSWPRINTF -D_IN_KERNEL_ -D_M_IX86 -D_NEW_DELETE_OPERATORS_ -D_NTOSKRNL_ -D_NTSYSTEM_ -D_SEH_ENABLE_TRACE -D_SETUPAPI_VER=0x502 -D_USE_32BIT_TIME_T -D_USE_PSEH3=1 -D_WIN32_IE=0x600 -D_WIN32_WINDOWS=0x502 -D_WIN32_WINNT=0x502 -D_X86_ -D__NTOSKRNL__ -D__REACTOS__ -D__ROS_ROSSYM__ -D__USE_PSEH2__ -D__i386__ -D_inline=__inline -Di386  -Intoskrnl -I../ntoskrnl -I../sdk/include -I../sdk/include/psdk -I../sdk/include/dxsdk -Isdk/include -Isdk/include/psdk -Isdk/include/dxsdk -Isdk/include/ddk -Isdk/include/reactos -Isdk/include/reactos/mc -I../sdk/include/crt -I../sdk/include/ddk -I../sdk/include/ndk -I../sdk/include/reactos -I../sdk/include/reactos/libs -I../ -I../sdk/lib/drivers/arbiter -I../sdk/lib/cmlib -I../ntoskrnl/include -Intoskrnl/include -Intoskrnl/include/internal -I../sdk/include/reactos/drivers -I../sdk/lib/pseh/include -I../VolatileBlock/include -ffile-prefix-map=/media/sf_reactos= -ffile-prefix-map=../= -pipe -fms-extensions -fno-strict-aliasing -nostdinc -mstackrealign -Wno-microsoft -Wno-pragma-pack -fno-associative-math -fcommon -fno-builtin-stpcpy -gdwarf-2 -ggdb -march=pentium -mtune=generic -Wall -Wpointer-arith -Wno-char-subscripts -Wno-multichar -Wno-unused-value -Wno-unused-const-variable -Wno-unused-local-typedefs -Wno-deprecated -O1 -fno-optimize-sibling-calls -fno-omit-frame-pointer -std=gnu99 -MD -MT ntoskrnl/CMakeFiles/ntoskrnl.dir/cc/pin.c.obj -MF ntoskrnl/CMakeFiles/ntoskrnl.dir/cc/pin.c.obj.d -fno-color-diagnostics -Xclang -ast-dump -c ../ntoskrnl/cc/pin.c
