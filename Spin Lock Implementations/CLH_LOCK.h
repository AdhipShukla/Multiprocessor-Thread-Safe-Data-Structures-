#ifndef _CLH_MUTEX_H_
#define _CLH_MUTEX_H_
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/* Compile read-write barrier */
#define barrier() asm volatile("": : :"memory")

/* Pause instruction to prevent excess processor bus usage */
#define cpu_relax() asm volatile("pause\n": : :"memory")

#define cmpxchg(P, O, N) __sync_val_compare_and_swap((P), (O), (N))

static inline void *xchg_ptr(void *ptr, void *x)
{
	__asm__ __volatile__("xchgq %0,%1"
				:"=r" ((unsigned long long) x)
				:"m" (*(volatile long long *)ptr), "0" ((unsigned long long) x)
				:"memory");

	return x;
}

typedef struct clh_node clh_node_t;

struct clh_node
{
    char MyLock;
};

typedef struct
{
    clh_node_t * mynode;
    char padding[64];  // To avoid false sharing with the tail
    clh_node_t *tail;
} clh_lock_t;

static clh_node_t * clh_make_node(char Locked)
{
    clh_node_t * new_node = (clh_node_t *)malloc(sizeof(clh_node_t));
    new_node->MyLock= Locked;
    return new_node;
}

void clh_mutex_init(clh_lock_t * Curr)
{
    clh_node_t * node = clh_make_node(0);
    Curr->mynode = node;
    Curr->tail= node;
}

void CLHlock(clh_lock_t * Curr)
{
    // Create the new node locked by default, setting islocked=1
    clh_node_t *mynode = clh_make_node(1); // Getting new node for current instance of the lock
    clh_node_t *prev = (clh_node_t *)xchg_ptr(&Curr->tail, mynode); // Seting tail to latest node atomically 
    barrier();
    while (prev->MyLock) {
        cpu_relax();//busy wait
    }
    free(prev); // Freeing up predecessor as it has relseased lock and its memory can be reused now
    Curr->mynode = mynode;//setting lock's mynode to self as it is used in unlock
}
void CLHunlock(clh_lock_t * Curr)
{
    barrier();
    Curr->mynode->MyLock=0;
}

void clh_mutex_destroy(clh_lock_t * Curr)
{
    free(Curr->tail);
}

#endif /* _CLH_MUTEX_H_ */ 