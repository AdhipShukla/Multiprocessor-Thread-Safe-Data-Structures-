CFLAGS = -O2 -g -std=gnu99 -Wall
LDFLAGS = -lpthread

programs = test-spinlock-xchg test-spinlock-pthread test-spinlock-cmpxchg test-spinlock-BackOff test-spinlock-MCS test-spinlock-CLH

all: $(programs)

test-spinlock-xchg: test-spinlock.c
	$(CC) $(CFLAGS) -DXCHG $^ -o $@ $(LDFLAGS)

test-spinlock-pthread: test-spinlock.c
	$(CC) $(CFLAGS) -DPTHREAD $^ -o $@ $(LDFLAGS)

test-spinlock-cmpxchg: test-spinlock.c
	$(CC) $(CFLAGS) -DCMPXCHG $^ -o $@ $(LDFLAGS)

test-spinlock-BackOff: test-spinlock.c
	$(CC) $(CFLAGS) -DBackOff $^ -o $@ $(LDFLAGS)

test-spinlock-MCS: test-spinlock.c
	$(CC) $(CFLAGS) -DMCS $^ -o $@ $(LDFLAGS)

test-spinlock-CLH: test-spinlock.c
	$(CC) $(CFLAGS) -DCLH $^ -o $@ $(LDFLAGS)

%:%.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	-rm -f *.o
	-rm -f $(programs)
