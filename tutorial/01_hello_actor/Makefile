
PWD=$(shell pwd)

# set your Simplx path
SIMPLX_PATH=$(PWD)/../..

SRC=$(shell find $(SIMPLX_PATH)/src/engine -iname "*.cpp")
OBJ=$(SRC:.cpp=.o)
INCLUDES= -I$(SIMPLX_PATH)/include
CFLAGS = -g -O0 -Wall -Wextra --std=c++11 -Wno-unused-parameter -Wno-parentheses -Wno-noexcept-type
LDFLAGS= -lpthread

# here is the name of your executable file
all: hello_actor.bin

%.o : %.cpp Makefile
	g++ -c $< -o $@ $(CFLAGS) $(INCLUDES)
	
hello_actor.bin : $(OBJ) Makefile $(PWD)/hello_actor.o
	g++ -o hello_actor.bin hello_actor.o $(OBJ) $(LDFLAGS)
	
.PHONY: clean

clean:
	rm -f $(OBJ) hello_actor.o hello_actor.bin