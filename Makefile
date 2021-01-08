CC = g++
CFLAGS += -Wall -g -std=c++14 `pkg-config --cflags thallium`
LDFLAGS += `pkg-config --libs thallium`

all:: raft 

raft: raft.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) raft.cpp -o raft.out