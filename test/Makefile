#
# Realtek Semiconductor Corp.
#
## Steve Liu (steve_liu@realsil.com.cn)
# Dec. 11, 2018
#


PWD							:= $(shell pwd)
SDK_PATH					:= $(abspath $(PWD)/../../../../../../rts390x_sdk_v2.3.7/)
DIR_LIB						:= $(abspath $(PWD)/../)
DIR_TMPFS					:= $(SDK_PATH)/tmpfs/
STSTREAM_INC				:= $(SDK_PATH)/users/rtscore/rtstream/rtstream/.formosa/build/include/
LIBRTSCAM_INC				:= $(SDK_PATH)/users/rtscore/librtscam/librtscamkit/.formosa/build/include/
MIDWARE_INC					:= $(SDK_PATH)/users/ysx/middle_ware/ysx_middle/include/
SDK_INC						:= -I$(STSTREAM_INC)
SDK_INC						+= -I$(LIBRTSCAM_INC)
SDK_INC						+= -I$(MIDWARE_INC)

TARGETS						:=  video_test audio_test sys_test
CFLAGS						:= -Wall -Os -std=gnu99
CFLAGS						+= $(SDK_INC)
.PHONY: all install $(TARGETS)
all: $(TARGETS)

romfs:
#	$(ROMFSINST) include/*		/bin/aac_play
#	$(ROMFSINST) lib/libysxlog.so /lib/


clean:
	rm -rf *.o
	rm -rf $(TARGETS)
#	rm -rf ./bin

$(TARGETS): %: %.o
	@if [ -e $(DIR_LIB)/libysxmiddle.a ]; then \
		echo -e "\033[32mmake $@ static\033[0m"; \
		$(CC) -o $@ $^ $(CFLAGS) -L$(DIR_LIB)/ -lysxmiddle $(LINKOPT); \
	fi
	@if [ -e $(DIR_LIB)/libysxmiddle.so ]; then \
		echo -e "\033[32mmake $@ dynamic\033[0m"; \
		$(CC) -o $@ $(CFLAGS) $^ -L$(DIR_LIB)/ -lysxmiddle -Wl,-rpath-link $(DIR_TMPFS)/lib/ \
			-Wl,-rpath-link; \
	fi

install:
	@if [ -e $(DIR_LIB)/libysxmiddle.a ]; then \
		echo -e "\033[32minstall $@ static\033[0m"; \
		mkdir -p $(PREFIX)/test/bin/static; \
		cd $(PREFIX)/test; \
		for i in "$(TARGETS)"; \
		do mv $$i $(PREFIX)/test/bin/static; done \
	fi
	@if [ -e $(DIR_LIB)/libysxmiddle.so ]; then \
		echo -e "\033[32minstall $@ dynamic\033[0m"; \
		mkdir -p $(PREFIX)/test/bin/dynamic; \
		cd $(PREFIX)/test; \
		for i in "$(TARGETS)"; \
		do mv $$i $(PREFIX)/test/bin/dynamic; done \
	fi

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@
