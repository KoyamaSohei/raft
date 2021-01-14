CC = g++
CFLAGS += -Wall -g -std=c++14 `pkg-config --cflags thallium`
LDFLAGS += `pkg-config --libs thallium`

all:: raft.out

raft.out: raft.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) provider.cpp raft.cpp  -o raft.out