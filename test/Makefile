TARGETS =  video_test  
#audio_test isp_test peripheral_test
ifneq ($(filter -DPWMDAC_DRV, $(CFLAGS)),)
	TARGETS += pwmdac_test
endif

.PHONY: all
all: $(TARGETS)

clean:
	-rm *.o
	-rm $(TARGETS)

$(TARGETS): %: %.o
	@echo -e "\033[32mmake $@ \033[0m"
#	echo "111"
#	echo $(LINKOPT)
#	echo $(PREFIX)/libs
	$(CC) -o $@ $^ -L.. -lysxmiddle -L$(PREFIX)/libs $(LINKOPT)
%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@
