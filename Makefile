CC = g++
CFLAGS += -Wall -g -std=c++14 `pkg-config --cflags thallium lmdb uuid`
LDFLAGS += `pkg-config --libs thallium lmdb uuid` -pthread

all:: raft.out raft_client.out test.out

raft.out: provider.cpp raft.cpp logger.cpp builder.cpp
	$(CC) $(CFLAGS)  provider.cpp raft.cpp logger.cpp fsm.cpp builder.cpp $(LDFLAGS) -o raft.out

raft_client.out: raft_client.cpp builder.cpp fsm.cpp
	$(CC) $(CFLAGS) raft_client.cpp builder.cpp fsm.cpp $(LDFLAGS) -o raft_client.out

test.out: *.cpp
	$(CC) $(CFLAGS) `pkg-config --cflags gtest gmock` \
	-coverage \
	fsm_test.cpp logger_test.cpp provider_test.cpp raft_test.cpp \
	provider.cpp logger.cpp fsm.cpp builder.cpp \
	$(LDFLAGS) `pkg-config --libs gtest gmock` \
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