TARGETS =  video_test  audio_test 
# sys_test

.PHONY: all
all: $(TARGETS)

clean:
	-rm *.o
	-rm $(TARGETS)

$(TARGETS): %: %.o
	@echo -e "\033[32mmake $@ \033[0m"
	echo $(CFLAGS)
#	echo $(PREFIX)/libs
	$(CC) -o $@ $^ -L.. -lysxmiddle -L$(PREFIX)/libs $(LINKOPT)
%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@
