CFLAGS = -O2 -g -std=gnu99 -Wall -w
LDFLAGS = -lpthread
nthrds ?= 16 
programs = Wait-Free-Queue-EnQTest Wait-Free-Queue-DeQTest Wait-Free-Queue-EnDeQTest Lock-Free-Queue-EnQTest Lock-Free-Queue-DeQTest Lock-Free-Queue-EnDeQTest

all: $(programs)

Wait-Free-Queue-EnQTest: WaitFreeQueue.c
	$(CC) $(CFLAGS) -DCorEnq -Dnthrds=$(nthrds) $^ -o $@ $(LDFLAGS)

Lock-Free-Queue-EnQTest: LockFreeQueue.c
	$(CC) $(CFLAGS) -DCorEnq -Dnthrds=$(nthrds) $^ -o $@ $(LDFLAGS)

Wait-Free-Queue-DeQTest: WaitFreeQueue.c
	$(CC) $(CFLAGS) -DCorDeq -Dnthrds=$(nthrds) $^ -o $@ $(LDFLAGS)

Lock-Free-Queue-DeQTest: LockFreeQueue.c
	$(CC) $(CFLAGS) -DCorDeq -Dnthrds=$(nthrds) $^ -o $@ $(LDFLAGS)

Wait-Free-Queue-EnDeQTest: WaitFreeQueue.c
	$(CC) $(CFLAGS) -DCorEnqPlusDeq -Dnthrds=$(nthrds) $^ -o $@ $(LDFLAGS)

Lock-Free-Queue-EnDeQTest: LockFreeQueue.c
	$(CC) $(CFLAGS) -DCorEnqPlusDeq -Dnthrds=$(nthrds) $^ -o $@ $(LDFLAGS)

%:%.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	-rm -f *.o
	-rm -f $(programs)
