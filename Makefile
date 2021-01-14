CC = g++
CFLAGS += -Wall -g -std=c++14 `pkg-config --cflags thallium lmdb`
LDFLAGS += `pkg-config --libs thallium lmdb`

all:: raft.out

raft.out: raft.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) provider.cpp raft.cpp logger.cpp  -o raft.out