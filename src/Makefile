CC=g++
CFLAGS=-Wall -g

SIM_SRC  = sim.cpp pipeline.cpp bpred.cpp 
SIM_OBJS = $(SIM_SRC:.cpp=.o)

all: $(SIM_SRC) sim

%.o: %.cpp 
	$(CC) $(CFLAGS) -c $< -o $@   

sim: $(SIM_OBJS) 
	$(CC) -o $@ $^

clean: 
	rm sim *.o
