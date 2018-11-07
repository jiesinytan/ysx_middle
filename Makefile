DIR_ROOT ?= /home/steve_liu/ipcam_linux/sdk_v2.3.1_release
DIR_TMPFS := $(DIR_ROOT)/tmpfs
CC := $(DIR_ROOT)/toolchain/rsdk/bin/rsdk-linux-gcc
AR := $(DIR_ROOT)/toolchain/rsdk/bin/rsdk-linux-ar
DIR_SRC := $(shell pwd)
PREFIX ?= $(DIR_SRC)

EXTRA_CFLAGS :=
CFLAGS += -Wall -Os -std=gnu99 -I$(DIR_TMPFS)/include -I$(DIR_SRC)/include \
	-I$(DIR_SRC) \
#	  -DCONFIG_REAL_TIME_STAMP
CFLAGS += $(EXTRA_CFLAGS)

STATICLIBS = rtsisp h1encoder rtsjpeg rtsosd2 \
	rtsosd rtsv4l2 rtscamkit \
	rtsacodec rtsaec rtsmp3 opencore-amrnb aacenc sbc opus \
	rtsio rtsgeom rtsamixer asound rtsresample rtstream rtsmd rtsbmp \
	motion_tracking

LINKLIBS = -lrtstream -lrtsresample -lrtsacodec -lrtsamixer -lrtsisp \
	-lh1encoder -lrtsjpeg -lrtsosd2 -lrtsosd -lrtsv4l2 \
	-lrtscamkit -lrtsaec -lrtsmp3 -lopencore-amrnb -laacenc -lsbc -lopus \
	-lrtsio -lrtsgeom -lasound -lrtsbmp -lrtsmd -lmotion_tracking

LINKOPT = -Wl,-Bstatic -Wl,--whole-archive -lrtstream -Wl,--no-whole-archive \
	  -lrtsacodec -lrtsamixer $(LINKLIBS) \
	  -Wl,-Bdynamic -ldl -lc -lpthread -lm

OBJS = ysx_video_middle.o ysx_audio_middle.o ysx_sys_middle.o

export CC CFLAGS LIB_PATH PREFIX LINKOPT DIR_SRC DIR_TMPFS

.PHONY: static share test clean
static:libysxmiddle.a
share: libysxmiddle.so

libysxmiddle.a: $(OBJS)
	@echo -e "\033[32mlink $@ \033[0m"
	$(AR) -crs $@ $^

libysxmiddle.so: $(OBJS)
	$(CC) --shared -o $@ $^ -L$(DIR_SRC)/libs $(LINKOPT)

install:
	mkdir -p $(PREFIX)/libs
	cd $(DIR_TMPFS)/lib; \
		for i in "$(patsubst %, lib%.a, $(STATICLIBS))"; do cp $$i $(PREFIX)/libs; done

test:
	$(MAKE) -C test

clean:
	-rm *.o
	-rm *.a
	-rm *.so
	$(MAKE) -C test clean

%.o: %.c
	@echo -e "\033[32mcompile $@ \033[0m"
	$(CC) $(CFLAGS) -c $^ -o $@
