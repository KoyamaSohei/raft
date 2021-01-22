CC = g++
CFLAGS += -Wall -g -std=c++14 `pkg-config --cflags thallium lmdb jsoncpp gtest`
LDFLAGS += `pkg-config --libs thallium lmdb jsoncpp gtest` -pthread

all:: raft.out raft_client.out test.out

raft.out: provider.cpp raft.cpp logger.cpp 
	$(CC) $(CFLAGS)  provider.cpp raft.cpp logger.cpp kvs.cpp $(LDFLAGS) -o raft.out

raft_client.out: raft_client.cpp 
	$(CC) $(CFLAGS) raft_client.cpp $(LDFLAGS) -o raft_client.out

test.out: *.cpp
	$(CC) $(CFLAGS) \
	-coverage \
	logger_test.cpp provider_test.cpp raft_test.cpp \
	provider.cpp logger.cpp kvs.cpp \
	$(LDFLAGS) \
	-o test.out

clean:
	rm *.out && \
	rm *.gcno && \
	rm *.gcda && \
	rm -r html && \
	rm -r log-*

test: test.out
	./test.out

cov:
	lcov -c -b . -d . -o cov_test.info && \
	genhtml --demangle-cpp -o html cov_test.info