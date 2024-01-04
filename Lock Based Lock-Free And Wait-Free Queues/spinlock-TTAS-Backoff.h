#include <stdlib.h>
#include <time.h>
//#include <pthread.h>
#include <unistd.h>
// ...

#define MIN_BACKOFF_DELAY 1000 // Minimum backoff delay in microseconds
#define MAX_BACKOFF_DELAY 1000000 // Maximum backoff delay in microseconds

#ifndef _SPINLOCK_XCHG_H
#define _SPINLOCK_XCHG_H

/* Spin lock using xchg.
 * Copied from http://locklessinc.com/articles/locks/
 */

/* Compile read-write barrier */
#define barrier() asm volatile("": : :"memory")

/* Pause instruction to prevent excess processor bus usage */
#define cpu_relax() asm volatile("pause\n": : :"memory")

static inline unsigned short xchg_8(void *ptr, unsigned char x)
{
    __asm__ __volatile__("xchgb %0,%1"
                :"=r" (x)
                :"m" (*(volatile unsigned char *)ptr), "0" (x)
                :"memory");

    return x;
}

#define BUSY 1
typedef unsigned char spinlock;

#define SPINLOCK_INITIALIZER 0

static inline void spin_lock(spinlock *lock)
{
    unsigned int backoff_delay = MIN_BACKOFF_DELAY; // Initial backoff delay
    unsigned int Maxtemp = MIN_BACKOFF_DELAY;
    while (1) {
        if (!xchg_8(lock, BUSY)) return;//xchg_8 is swapping the value of BUSY and xchg_8 and return the old value of xchg_8
        //If the lock is currently zero the xchg_8 will set it to 1 and return previous value zero. So if condition will be true and thread will return from busy waiting to enter CS
        //If the lock is currently one the xchg_8 will set it to 1(no need but still exchnage) and return the previous value which is 1. So if will fail and thread will do busy-loop
        else{
            usleep(backoff_delay); // Sleep for a random time within backoff_delay
            backoff_delay=rand();
            if (backoff_delay>Maxtemp || backoff_delay>MAX_BACKOFF_DELAY){
                backoff_delay = Maxtemp;   
                if (backoff_delay>MAX_BACKOFF_DELAY)
                    backoff_delay = MAX_BACKOFF_DELAY;
            }
            else if(backoff_delay<MIN_BACKOFF_DELAY)
                backoff_delay = MIN_BACKOFF_DELAY;

            Maxtemp = Maxtemp * 2;
        }
        while (*lock) cpu_relax(); //If the lock is 1 or CS is occupied then apply the cpu_relax(efficient busy waiting), as soon as lock is released loop back to check the lock value in if condition
        /*usleep(backoff_delay); // Sleep for a random time within backoff_delay
        backoff_delay=rand()%(Maxtemp-MIN_BACKOFF_DELAY+1)+MIN_BACKOFF_DELAY;
        Maxtemp = Maxtemp * 2;
        */
    }
}

static inline void spin_unlock(spinlock *lock)
{
    barrier();
    *lock = 0;
}

static inline int spin_trylock(spinlock *lock)
{
    return xchg_8(lock, BUSY);
}

#endif /* _SPINLOCK_XCHG_H */



/*
static inline void spin_lock(spinlock *lock)
{
    

    while (1) {
        if (!xchg_8(lock, BUSY)) return; // Try to acquire the lock

        // Lock acquisition failed, apply backoff and retry
         // Double the backoff delay
        
    }
}
*/