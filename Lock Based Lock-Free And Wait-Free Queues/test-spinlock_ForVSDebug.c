#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <errno.h>
#define BackOff 1
#define XCHG 0
#define PTHREAD 0
#define CMPXCHG 0
#define MCS 0
#define CLH 0
/*
#ifdef XCHG
    #include "spinlock-xchg.h"
#elif defined(PTHREAD)
    #include "spinlock-pthread.h"
#elif defined(CMPXCHG)
    #include "spinlock-cmpxchg.h"
#else
    #error "must define a spinlock implementation"
#endif
*/

#if (BackOff==1)
    #include "spinlock-TTAS-Backoff.h"
#elif (XCHG==1)
    #include "spinlock-xchg.h"
#elif (PTHREAD==1)
    #include "spinlock-pthread.h"
#elif (CMPXCHG==1)
    #include "spinlock-cmpxchg.h"
#elif (MCS==1)
    #include "MCSLock.h"
#elif (CLH==1)
    #include "CLH_LOCK.h"
#else
    #error "must define a spinlock implementation"
#endif

#ifndef cpu_relax
#define cpu_relax() asm volatile("pause\n": : :"memory")
#endif

struct QNode{
    int Value;
    struct QNode *Next;
};
typedef struct QNode QN;

struct CurrQueue{
    QN *Front, *Rear;
};
typedef struct CurrQueue CQue;

int dequeue(CQue* CurQue);
void enqueue(CQue* CurQue, int val);
QN *Newnode(int k);
CQue* CreateQueue();
CQue* Q=NULL;
int En=1;
/*
 * You need  to provide your own code for bidning threads to processors
 * Use lscpu on rlogin servers to get an idea of the number and topology
 * of processors. The bind_
 */

/* Number of total lock/unlock pair.
 * Note we need to ensure the total pair of lock and unlock opeartion are the
 * same no matter how many threads are used. */
#define N_PAIR 1000000//16000000 //15999968

/* Bind threads to specific cores. The goal is to make threads locate on the
 * same physical CPU. Modify bind_core before using this. */
//#define BIND_CORE

static int nthr = 0;

static volatile uint32_t wflag; // Making sure that acquire lock is atomic and complier does not optimize this variable implementation as it is a inter-thread variable

/* Wait on a flag to make all threads start almost at the same time. */
void wait_flag(volatile uint32_t *flag, uint32_t expect) {//expect is number of threads flag is updated with each call
    __sync_fetch_and_add((uint32_t *)flag, 1);
    while (*flag != expect) {// All threads looping here and cehcking when the flag has reached expected
        cpu_relax();
    }
}

static struct timeval start_time;
static struct timeval end_time;

static float calc_time(struct timeval *start, struct timeval *end) {
    if (end->tv_usec < start->tv_usec) {
        end->tv_sec -= 1;
        end->tv_usec += 1000000;
    }
    assert(end->tv_sec >= start->tv_sec);
    assert(end->tv_usec >= start->tv_usec);
    struct timeval interval = {
        end->tv_sec - start->tv_sec,
        end->tv_usec - start->tv_usec
    };
    printf("Time taken: %ld.%06ld\n", (long)interval.tv_sec, (long)interval.tv_usec);
    float totalSec=interval.tv_sec+interval.tv_usec*0.000001;//microsec
    return totalSec;
}

// Use an array of counter to see effect on RTM if touches more cache line.
#define NCOUNTER 1
#define CACHE_LINE 64

// Use thread local counter to avoid cache contention between cores.
// For TSX, this avoids TX conflicts so the performance overhead/improvement is
// due to TSX mechanism.
//static __thread int8_t counter[CACHE_LINE*NCOUNTER];
static __volatile int32_t counter[CACHE_LINE*NCOUNTER];
//#ifdef MCS
#if MCS == 1
mcs_lock cnt_lock = NULL; // Initializing the Tail to NULL 
#elif CLH ==1
//CLH_Lock LockCLH;
clh_lock_t LockCLH;
#else
spinlock slEn, slDe;
#endif

#ifdef BIND_CORE
void bind_core(int threadid) {
    /* cores with logical id 2x   are on node 0 */
    /* cores with logical id 2x+1 are on node 1 */
    /* each node has 16 cores, 32 hyper-threads */
    int phys_id = threadid / 16;
    int core = threadid % 16;

    int logical_id = 2 * core + phys_id;
    /*printf("thread %d bind to logical core %d on physical id %d\n", threadid, logical_id, phys_id);*/

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(logical_id, &set);

    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        perror("Set affinity failed");
        exit(EXIT_FAILURE);
    }
}
#endif

void *inc_thread(void *id) { //Fucntion that thread will work on
    int n = N_PAIR / nthr;
    assert(n * nthr == N_PAIR);
#if MCS ==1
    mcs_lock_t local_lock;
#elif CLH ==1
//    CLH_Lock local_lock;
#endif
#ifdef BIND_CORE
    bind_core((int)(long)(id));
#endif
    wait_flag(&wflag, nthr);

    if (((long) id == 0)) {
        /*printf("get start time\n");*/
        gettimeofday(&start_time, NULL);
    }

    /* Start lock unlock test. */
    for (int i = 0; i < n; i++) {
#if MCS ==1
        lock_mcs(&cnt_lock, &local_lock);
        for (int j = 0; j < NCOUNTER; j++) 
            counter[j*CACHE_LINE]++;
        unlock_mcs(&cnt_lock, &local_lock);
#elif CLH==1
        CLHlock(&LockCLH);
        for (int j = 0; j < NCOUNTER; j++) 
            counter[j*CACHE_LINE]++;
        CLHunlock(&LockCLH);
#elif RTM
        int status;
        if ((status = _xbegin()) == _XBEGIN_STARTED) {
            for (int j = 0; j < NCOUNTER; j++) counter[j*CACHE_LINE]++;
            if (sl == BUSY)
                _xabort(1);
            _xend();
        } else {
            spin_lock(&sl);
            for (int j = 0; j < NCOUNTER; j++) counter[j*CACHE_LINE]++;
            spin_unlock(&sl);
        }
#else
        En=rand()%2;
        if (i%2 == 0){
        spin_lock(&slEn);
        for (int j = 0; j < NCOUNTER; j++){ 
            counter[j*CACHE_LINE]++;
            enqueue(Q,Q->Rear->Value+1);
        }
        spin_unlock(&slEn);
        }
        else{
            spin_lock(&slDe);
            for (int j = 0; j < NCOUNTER; j++){ 
            counter[j*CACHE_LINE]++;
            if(Q->Front!=Q->Rear){
                dequeue(Q);
                }
            }
            spin_unlock(&slDe);
        }
#endif
    }

    if (__sync_fetch_and_add((uint32_t *)&wflag, -1) == 1) {
        /*printf("get end time\n");*/
        gettimeofday(&end_time, NULL);
    }
    return NULL;
}

int main(int argc, const char *argv[])
{

    Q=CreateQueue();
    enqueue(Q,0);
    pthread_t *thr; // Defining a variable thr which is pointer to a thread ID
    int ret = 0;
    
    argc=2; // Adding argumets for the sake of using debugger
    *(argv+1)="16";
    
    if (argc != 2) {
        printf("Usage: %s <num of threads>\n", argv[0]);
        exit(1);
    }
    #if CLH ==1
    //CLH_Lock Firstlock;
    //initCLHLock(&LockCLH);
    clh_mutex_init(&LockCLH); // Initializing the lock pointer
    #endif
    nthr = atoi(argv[1]); // Getting the number of threads
    /*printf("using %d threads\n", nthr);*/
    thr = calloc(sizeof(*thr), nthr); // Assigning the memory for pointer to IDs of all the threads

    // Start thread
    for (long i = 0; i < nthr; i++) { // creating all the threads
        if (pthread_create(&thr[i], NULL, inc_thread, (void *)i) != 0) {
            perror("thread creating failed");
        }
    }
    // join thread
    for (long i = 0; i < nthr; i++)
        pthread_join(thr[i], NULL);

    float TotalTime=calc_time(&start_time, &end_time);
    printf("Throughpt of enqueues plus dequeues: %f\n",N_PAIR/TotalTime);//Just counting last half million deque and enque each
    #if CLH ==1
    clh_mutex_destroy(&LockCLH); // Freeing the lock pointer of CLH
    #endif

    /*for (int i = 0; i < NCOUNTER; i++) {
        if (counter[i] == N_PAIR) {
            printf("Correct Algo\n");
        } else {
            printf("counter %d error\n", i);
            ret = 1;
        }
    }*/
    while(1){
        if(Q->Front==NULL)
            break;
        printf("Remaining Elements in Queue: %d\n",Q->Front->Value);
        dequeue(Q);
    }

    return ret;
}

QN *Newnode(int k){
    QN *temp =(QN*)malloc(sizeof(QN*));
    temp->Next = NULL;
    temp->Value = k;
    return temp; 
}

CQue* CreateQueue(){
    CQue* temp =(CQue*)malloc(sizeof(CQue*));
    temp->Front=NULL;
    temp->Rear=NULL;
    return temp;
}

void enqueue(CQue* CurQue, int val){
    
    QN* NewNode = Newnode(val);
    if (CurQue->Rear == NULL){
        CurQue->Front=NewNode;
        CurQue->Rear=NewNode;
        return;
    }
    CurQue->Rear->Next = NewNode; // making predecessor node to point at new node
    CurQue->Rear = NewNode; //changing where the  rear node is poinintg
}

int dequeue(CQue* CurQue){
    if  (CurQue->Front == NULL)
        return 'E';
    QN* temp = CurQue->Front;
    CurQue->Front=CurQue->Front->Next;
    if (CurQue->Front==NULL){
        CurQue->Rear=NULL;
    }
    int value=temp->Value;
    free(temp);
    return value;
}
