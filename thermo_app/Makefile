
MACHINE = $(shell uname -m)

CC = g++
CPPFLAGS = -Wall -O2 -flto -pthread -std=gnu++11
LFLAGS = -pthread -flto
LIBS = -lpq -lrt
SRCS = log_trace_debug.cpp pgdbwrapper.cpp exporter.cpp therm.cpp data.cpp thread_util.cpp
#SRCS = *.cpp
OBJS = $(SRCS:.cpp=.o) 
MAIN = app

	INCLUDES = -I../owtherm

#
# The following part of the makefile is generic; it can be used to 
# build any executable just by changing the definitions above and by
# deleting dependencies appended to the file from 'make depend'
#
.PHONY: depend clean

all:    $(MAIN)
	@echo `date` compiled on $(MACHINE)

$(MAIN): $(OBJS) 
	$(CC) $(CPPFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.cpp.o:
	$(CC) $(CPPFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
