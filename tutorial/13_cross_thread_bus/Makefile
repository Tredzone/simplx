PWD=$(shell pwd)
PATH_SIMPLX= $(PWD)/../..
INCLUDES_SIMPLX= -I $(PATH_SIMPLX)/include

SRC_SIMPLX=$(shell find $(PATH_SIMPLX)/src/engine -iname "*.cpp")
SRC_UTILS=$(shell find $(PATH_SIMPLX)/src/util -iname "*.cpp")

V ?= 1
SRC=$(SRC_SIMPLX) $(SRC_UTILS) main.v$(V).cpp

FLAGS = -g -Wall -Wextra --std=c++11 -fno-access-control -Wno-unused-parameter -Wno-parentheses -Wfatal-errors

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    FLAGS+= -O0
else
    FLAGS+= -O2 -DNDEBUG
endif

OBJ= $(SRC:.cpp=.o) $(OBJ_ZLIB)

DEP = $(OBJ:.o=.d) 

LDFLAGS=-lpthread

all: main

%.o: %.c Makefile
	gcc -O3 -c $< -o $@

%.o: %.cpp %.d Makefile
	g++ -c $< -o $@ $(FLAGS) $(INCLUDES_SIMPLX)

-include $(DEP)

%.d: %.cpp Makefile
	g++ $(FLAGS) $(INCLUDES_SIMPLX) $< -MM -MT $(@:.d=.o) >$@


main: $(OBJ) Makefile
	g++ -o main.v$(V).bin $(OBJ) $(LDFLAGS)

.PHONY: clean

clean: 
	rm -f $(DEP)
	rm -f $(OBJ)
	rm -f *.d
	rm -f *.o
	rm -f main.v*.bin