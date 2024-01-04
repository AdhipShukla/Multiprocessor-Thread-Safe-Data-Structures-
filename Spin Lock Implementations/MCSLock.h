#ifndef _MCS_Lock
#define _MCS_Lock
#include <stdio.h>
#include <stdbool.h>
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

static inline void *CompExchPoi(void *ptr1, void *ptr2)
{
    void *readval =ptr2;
    asm volatile (
            "lock; cmpxchgb %b2, %0"
            : "+m" (ptr1), "+a" (readval)
            : "r" (1)
            : "cc");
    return readval;
}

typedef struct MCS_Lock_t{
    struct MCS_Lock_t *Next;
    bool MyLock; 
}mcs_lock_t;

typedef mcs_lock_t* mcs_lock;//Tail of the queue. Always points to the latest node

#define SPINLOCK_INITIALIZER 0

static inline void lock_mcs(mcs_lock *LockPoi, mcs_lock_t *MyNode)
{
    mcs_lock Prev=(mcs_lock)xchg_ptr(LockPoi,MyNode);//Get previous node pointed by Q and set Q to me
    if(Prev!= NULL){
        MyNode->MyLock=true;
        Prev->Next=MyNode;
        barrier();
        while(MyNode->MyLock){
            cpu_relax();//busy wait
        }
    }
}

static inline void unlock_mcs(mcs_lock *LockPoi, mcs_lock_t *MyNode)
{
    if(MyNode->Next== NULL){
        if(cmpxchg(LockPoi,MyNode,NULL)==MyNode){//Mynode is connected by Tail
            return;
        }
        barrier();
        while (MyNode->Next==NULL){//Let the new node attach
            cpu_relax();//busy wait
        }
    }
    MyNode->Next->MyLock=false;
    MyNode->Next=NULL;
}
/*
static inline int spin_trylock(spinlock *lock)
{
    return xchg_8(lock, BUSY);
}
*/
#endif /* _SPINLOCK_XCHG_H */
