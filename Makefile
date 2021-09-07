CXXFLAGS += -g -O1 -std=c++20 -Wall -fno-exceptions

headers := $(wildcard *.h)
sources := $(wildcard *.cc)

CLANG_FORMAT ?= clang-format

main: main.o json.o parse.o unicode.o
	$(CXX) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

.PHONY: format
format:
	$(CLANG_FORMAT) --style=Google -i $(headers) $(sources)

.PHONY: clean
clean:
	rm -f *.o *.d main

include $(sources:.cc=.d)

%.d: %.cc
	@set -e; rm -f $@; \
	 $(CXX) -M $(CPPFLAGS) $< > $@.$$$$; \
	 sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	 rm -f $@.$$$$
