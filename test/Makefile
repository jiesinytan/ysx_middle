TARGETS =  video_test audio_test sys_test

.PHONY: all install
all: install
install: $(TARGETS)


clean:
	-rm *.o
	-rm $(TARGETS)
	-rm -rf ./bin

$(TARGETS): %: %.o
	@if [ -e $(DIR_SRC)/libysxmiddle.a ]; then \
		echo -e "\033[32mmake $@ static\033[0m"; \
		$(CC) -o $@ $^ -L.. -lysxmiddle -L$(DIR_SRC)/libs $(LINKOPT); \
	fi
	@if [ -e $(DIR_SRC)/libysxmiddle.so ]; then \
		echo -e "\033[32mmake $@ dynamic\033[0m"; \
		$(CC) -o $@ $^ -L.. -lysxmiddle -Wl,-rpath-link $(DIR_SRC)/libs; \
	fi

install:
	@if [ -e $(DIR_SRC)/libysxmiddle.a ]; then \
		echo -e "\033[32minstall $@ static\033[0m"; \
		mkdir -p $(PREFIX)/test/bin/static; \
		cd $(PREFIX)/test; \
		for i in "$(TARGETS)"; \
		do mv $$i $(PREFIX)/test/bin/static; done \
	fi
	@if [ -e $(DIR_SRC)/libysxmiddle.so ]; then \
		echo -e "\033[32minstall $@ dynamic\033[0m"; \
		mkdir -p $(PREFIX)/test/bin/dynamic; \
		cd $(PREFIX)/test; \
		for i in "$(TARGETS)"; \
		do mv $$i $(PREFIX)/test/bin/dynamic; done \
	fi

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@
