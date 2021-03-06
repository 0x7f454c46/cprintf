PLUGIN		:= cprintf
PLUGIN_SO	:= $(addsuffix .so,$(PLUGIN))
OBJS		:= cprintf log printfun gcc_hell

PLUGIN_INCLUDE	:= $(shell gcc -print-file-name=plugin)
ifeq ($(PLUGIN_INCLUDE),plugin)
        $(error Error: GCC API for plugins is not installed on your system)
endif

CXX		:= g++
CC		:= gcc
CXXFLAGS	+= -I $(PLUGIN_INCLUDE)/include

all: $(PLUGIN_SO)

$(PLUGIN_SO): $(addsuffix .o,$(OBJS))
	$(CXX) $(LDFLAGS) -shared -fno-rtti -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -fPIC -fno-rtti -c -o $@ $<

clean:
	rm -f $(addsuffix .so,$(PLUGIN)) $(addsuffix .o,$(PLUGIN))
	rm -f ./test/quicksort
	rm -f ./test/crlog

check: $(PLUGIN_SO)
	$(CXX) -fplugin=./$(PLUGIN_SO) -c -x c++ /dev/null -o /dev/null	\
		-fplugin-arg-cprintf-log_level=Err			\
		-fplugin-arg-cprintf-printf="printf(0): %c putchar"
	$(CC) -fplugin=./$(PLUGIN_SO)					\
		./test/quicksort.c -o ./test/quicksort			\
		-fplugin-arg-cprintf-printf="printf(0): %d putchar %s puts"
	$(CC) ./test/crlog.c -o ./test/crlog
	./test/crlog > /dev/null
	$(CC) -fplugin=./$(PLUGIN_SO)					\
		./test/crlog.c -o ./test/crlog				\
		-fplugin-arg-cprintf-printf="printf(0): %s __puts	\
			%c putchar %li __putlong %d __putshort		\
			%lu __putulong %% __putwrite"
	./test/crlog > /dev/null

.PHONY: all clean check
