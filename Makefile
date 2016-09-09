PLUGIN		:= cprintf
PLUGIN_SO	:= $(addsuffix .so,$(PLUGIN))

PLUGIN_INCLUDE	:= $(shell gcc -print-file-name=plugin)
ifeq ($(PLUGIN_INCLUDE),plugin)
        $(error Error: GCC API for plugins is not installed on your system)
endif

CXX		:= g++
CC		:= gcc
CXXFLAGS	+= -I $(PLUGIN_INCLUDE)/include

define newline


endef

# map function:
# $1 - func to call
# $2 - list over which map the $1 func
# result is divided with newlines
map = $(foreach x,$2,$(call $1,$x)$(newline))

all: $(PLUGIN_SO)

%.so: %.o
	$(CXX) $(LDFLAGS) -shared -fno-rtti -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -fPIC -fno-rtti -c -o $@ $<

clean:
	rm -f $(addsuffix .so,$(PLUGIN)) $(addsuffix .o,$(PLUGIN))
	rm -f ./test/quicksort

check: $(PLUGIN_SO)
	$(CXX) -fplugin=./$(PLUGIN_SO) -c -x c++ /dev/null -o /dev/null	\
		-fplugin-arg-cprintf-log_level=Err			\
		-fplugin-arg-cprintf-printf="printf(1): %c putchar"
	$(CC) -fplugin=./$(PLUGIN_SO)					\
		./test/quicksort.c -o ./test/quicksort			\
		-fplugin-arg-cprintf-printf="printf(1): %c putchar"

.PHONY: all clean check
