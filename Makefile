#
# Realtek Semiconductor Corp.
#
## Steve Liu (steve_liu@realsil.com.cn)
# Dec. 11, 2018
#


CC := mips-linux-gcc
DIR_SRC := $(shell pwd)

EXTRA_CFLAGS := -fPIC
CFLAGS += -Wall -Os -std=gnu99 -I$(DIR_TMPFS)/include -I$(DIR_SRC)/include \
	-I$(DIR_SRC) \
#	  -DCONFIG_REAL_TIME_STAMP
CFLAGS += $(EXTRA_CFLAGS)
CFLAGS += -I$(DIR_SRC)/../../../../tmpfs/include/
SHARE_LINKLIBS := -lrtstream -lrtscamkit -lrtsamixer -lasound -lrtsio -lrtsisp
YSX_MID_DBG_MSG						?= y
ifeq ($(YSX_MID_DBG_MSG), y)
SHARE_LINKLIBS 						+= -lysxlog
CFLAGS								+= -DYSX_MID_DBG_MSG
endif


OBJS = ysx_video_middle.o ysx_audio_middle.o ysx_sys_middle.o

all: libysxmiddle.so tmpfs

libysxmiddle.so: $(OBJS)
	$(CC) -shared -o $@ $^ -Wl,-soname,libysxmiddle.so \
		-L$(DIR_TMPFS)/lib $(SHARE_LINKLIBS)

tmpfs:
	$(TMPFSINST) include/ /include/
	$(TMPFSINST) libysxmiddle.so /lib/

romfs:
	$(ROMFSINST) libysxmiddle.so /lib/

clean:
	-rm *.o
	-rm *.so

%.o: %.c
	@echo -e "\033[32mcompile $@ \033[0m"
	$(CC) $(CFLAGS) -c $^ -o $@
