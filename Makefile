CC = g++
CFLAGS += -Wall -g -std=c++14 `pkg-config --cflags thallium lmdb`
LDFLAGS += `pkg-config --libs thallium lmdb`

all:: raft.out raft_client.out

raft.out: provider.cpp raft.cpp logger.cpp 
	$(CC) $(CFLAGS) $(LDFLAGS) provider.cpp raft.cpp logger.cpp  -o raft.out

raft_client.out: raft_client.cpp 
	$(CC) $(CFLAGS) $(LDFLAGS) raft_client.cpp -o raft_client.out
