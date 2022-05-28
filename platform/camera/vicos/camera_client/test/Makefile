ANDROID_NDK_ROOT:=~/.anki/android/ndk-repository/android-ndk-r15b

ANDROID_ARM_TOOLS_ROOT:=../../../../../generated/android/tools/arm
ANDROID_ARM_TOOLS_BIN:=$(ANDROID_ARM_TOOLS_ROOT)/bin
# Compilation Tools
CC := $(ANDROID_ARM_TOOLS_BIN)/arm-linux-androideabi-gcc
CPP := $(ANDROID_ARM_TOOLS_BIN)/arm-linux-androideabi-g++
AR := $(ANDROID_ARM_TOOLS_BIN)/arm-linux-androideabi-ar
HOST_CC := /usr/bin/clang
# Compiler flags
# Specify all the flags below which you want to use for your compilation, For this simple example, we only need to specify the include directory path
CFLAGS          := -I..
CFLAGS          += -I../inc
CFLAGS          += -DPLATFORM_ANDROID=1
CFLAGS          += -DUSE_NEON_DOWNSAMPLE=0


# SRCS = processed_victor_camera.c
# OBJ =  processed_victor_camera.o
#SRCS = victor_camera.c camera_test_main.c
OBJ =  ../victor_camera.o camera_test_main.o

APP = camera_test

all: $(ANDROID_ARM_TOOLS_ROOT) $(APP)

$(ANDROID_ARM_TOOLS_ROOT):
	$(ANDROID_NDK_ROOT)/build/tools/make_standalone_toolchain.py --arch arm --install-dir $(ANDROID_ARM_TOOLS_ROOT) --api 24

.c.o: 
	$(CC) -c $(CFLAGS) -fno-exceptions -Wno-multichar -msoft-float -ffunction-sections -fdata-sections -funwind-tables -fstack-protector-strong -Wa,--noexecstack -Werror=format-security -D_FORTIFY_SOURCE=2 -fno-short-enums -no-canonical-prefixes -fno-canonical-system-headers -mcpu=cortex-a7 -D__ARM_FEATURE_LPAE=1 -mfloat-abi=softfp -mfpu=neon  -fno-builtin-sin -fno-strict-volatile-bitfields -Wno-psabi -mthumb-interwork -DANDROID -fmessage-length=0 -W -Wall -Wno-unused -Winit-self -Wpointer-arith -fdiagnostics-color -fdebug-prefix-map=/proc/self/cwd= -Werror=return-type -Werror=non-virtual-dtor -Werror=address -Werror=sequence-point -Werror=date-time -DNDEBUG -g -Wstrict-aliasing=2 -fgcse-after-reload -frerun-cse-after-loop -frename-registers -DNDEBUG -UDEBUG  -mthumb -Os -fomit-frame-pointer -fno-strict-aliasing   -DAMSS_VERSION= -DUSE_ION -D_ANDROID_ -DCAMERA_ION_HEAP_ID=ION_IOMMU_HEAP_ID -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_MM_HEAP -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP -DCAMERA_ION_FALLBACK_HEAP_ID=ION_IOMMU_HEAP_ID -DCAMERA_GRALLOC_CACHING_ID=0 -DNUM_RECORDING_BUFFERS=9 -Wall -Wextra -Werror -fpie -D_USING_LIBCXX   -Werror=int-to-pointer-cast -Werror=pointer-to-int-cast  -MD   -o $@ $<

$(APP):   $(OBJ)
	$(CC) -pie -llog $(OBJ) -o $(APP)

camera_client:	main_camera_client.c ../camera_client.c ../log.c
	$(CC) $(CFLAGS) -Os -std=c11 -DUSE_ANDROID_LOGGING -pie -pthread -llog -o camera_client ../camera_client.c ../log.c main_camera_client.c 

bayer2rgb: bayer2rgb.c
	$(HOST_CC) -std=c99 -O2 -o bayer2rgb bayer2rgb.c

clean:
	rm -f $(APP) *.o *.d
