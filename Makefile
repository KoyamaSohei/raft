CC = g++
CFLAGS += -Wall -g -std=c++14 `pkg-config --cflags thallium lmdb jsoncpp gtest`
LDFLAGS += `pkg-config --libs thallium lmdb jsoncpp gtest` -pthread

all:: raft.out raft_client.out test.out

raft.out: provider.cpp raft.cpp logger.cpp 
	$(CC) $(CFLAGS)  provider.cpp raft.cpp logger.cpp kvs.cpp $(LDFLAGS) -o raft.out

raft_client.out: raft_client.cpp 
	$(CC) $(CFLAGS) raft_client.cpp $(LDFLAGS) -o raft_client.out

test.out: test/*.cpp *.cpp
	$(CC) $(CFLAGS) \
	test/logger.cpp test/provider.cpp \
	provider.cpp logger.cpp kvs.cpp \
	$(LDFLAGS) \
	-o test.out

clean:
	rm -r log-* && \
	rm *.out

test: test.out
	./test.out