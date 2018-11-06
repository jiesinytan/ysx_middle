DIR_ROOT ?= /home/jiesiny/SDK/sdk_v2.3_release
DIR_TMPFS := $(DIR_ROOT)/tmpfs
CC := $(DIR_ROOT)/toolchain/rsdk/bin/rsdk-linux-gcc
AR := $(DIR_ROOT)/toolchain/rsdk/bin/rsdk-linux-ar
DIR_SRC := $(shell pwd)
PREFIX ?= $(DIR_SRC)

CFLAGS += -Wall -Os -std=gnu99 -I$(DIR_TMPFS)/include -I$(DIR_SRC)/include \
#	  -DCONFIG_REAL_TIME_STAMP

STATICLIBS = rtsisp h1encoder rtsjpeg rtsosd2 \
	rtsosd rtsv4l2 rtscamkit \
	rtsacodec rtsaec rtsmp3 opencore-amrnb aacenc sbc opus \
	rtsio rtsgeom rtsamixer asound rtsresample rtstream  \
	

LINKLIBS = -lrtsisp -lh1encoder -lrtsjpeg -lrtsosd2 -lrtsosd -lrtsv4l2 \
	-lrtscamkit -lrtsaec -lrtsmp3 -lopencore-amrnb -laacenc -lsbc -lopus \
	-lrtsio -lrtsgeom -lasound -lpthread -lm -ldl  \
	

LINKOPT = -Wl,--whole-archive -lrtstream -Wl,--no-whole-archive -lrtsresample \
	-lrtsacodec -lrtsamixer $(LINKLIBS)

OBJS = ysx_video_middle.o ysx_audio_middle.o

export CC CFLAGS LIBS LIB_PATH PREFIX LINKOPT

.PHONY: all test install clean nfs
all: libysxmiddle.a install

libysxmiddle.a: $(OBJS)
	@echo -e "\033[32mlink $@ \033[0m"
	$(AR) -crs $@ $^

test: $(TARGETS)
	$(MAKE) -C test

install:
	mkdir -p $(PREFIX)/libs
	cd $(DIR_TMPFS)/lib; \
		for i in "$(patsubst %, lib%.a, $(STATICLIBS))"; do cp $$i $(PREFIX)/libs; done

nfs:
	-mkdir -p ~/share/nfs/ysx_middle_test
	-cp *.a ~/share/nfs/ysx_middle_test
	-cp test/*_test ~/share/nfs/ysx_middle_test

clean:
	-rm *.o
	-rm *.a
	-rm ./libs -rf
	$(MAKE) -C test clean

%.o: %.c
	@echo -e "\033[32mcompile $@ \033[0m"
	$(CC) $(CFLAGS) -c $^ -o $@
