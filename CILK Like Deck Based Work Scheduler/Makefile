CC = gcc
CFLAGS = -O2 -g
TARGETS = DeckLockBased DeckLockFree FibDeckLockBased FibDeckLockFree QSortDeckLockBased QSortDeckLockFree

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)
