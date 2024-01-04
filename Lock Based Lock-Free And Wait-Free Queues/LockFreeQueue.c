#define _GNU_SOURCE
#include<pthread.h>
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <assert.h>
#define Nodes 1000000
#define default_nthrds 16
#ifndef nthrds
#define nthrds default_nthrds
#endif
//#define nthrds 16
//#define CorEnq

#define SPINLOCK_ATTR static __inline __attribute__((always_inline, no_instrument_function))
#define barrier() asm volatile("": : :"memory")
#ifndef cpu_relax
#define cpu_relax() asm volatile("pause\n": : :"memory")
#endif
#define CAS(P, O, N) __sync_val_compare_and_swap((P), (O), (N))

static volatile uint32_t wflag;

void wait_flag(volatile uint32_t *flag, uint32_t expect) {//expect is number of threads flag is updated with each call
    __sync_fetch_and_add((uint32_t *)flag, 1);
    while (*flag != expect) {// All threads looping here and cehcking when the flag has reached expected
        cpu_relax();
    }
}

static struct timeval start_time;
static struct timeval end_time;
static int TotalDqs=0;
static int *uniqueArrDeqTest;
static int SecondHalf=0;

struct MetaPtr{
    struct Node* ptr;
    int cnt;
};
typedef struct MetaPtr Mptr;

struct Node{
    int val;
    Mptr *next;
};
typedef struct Node node;

struct QueueLockFree{
    Mptr *Head;
    Mptr *Tail;
};
typedef struct QueueLockFree QLF;

QLF* Qu=NULL;

void CreateNode(node** N){
    *N= (node*) malloc(sizeof(node));//node*
    (*N)->next= (Mptr*) malloc(sizeof(Mptr));//Mptr*
}
Mptr* CreateMetaNode(){
    Mptr* M= (Mptr*) malloc(sizeof(Mptr));//Mptr*
    return M;
}
void InitQueue(QLF* Q){
    node* Sentinel;
    CreateNode(&Sentinel);
    Mptr* M=CreateMetaNode();
    Q->Head=M;
    Q->Tail=CreateMetaNode();
    Sentinel->next->ptr = NULL;
    Sentinel->val = -1;
    Q->Head->ptr = Sentinel;
    Q->Tail->ptr = Sentinel;
}

void enqueue(QLF *Q, int V){
    node* NewNode;
    CreateNode(&NewNode);//set the next->ptr to null
    NewNode->val = V;
    NewNode->next->ptr = NULL;
    Mptr* NewMeta=CreateMetaNode();// Next node's meta data in previous node
    Mptr* NextTail=CreateMetaNode();
    Mptr* MyTail;
    while(1){
        Mptr* tail = Q->Tail;
        Mptr* tailnext = tail->ptr->next;
        if (tail == Q->Tail){ // Is the tail still pointing to where I think
            if (tailnext->ptr == NULL){ //Is the pointer fieled of the next fieled of node pointed by tail NULL?
                NewMeta->ptr=NewNode;
                NewMeta->cnt++;
                    if (CAS(&tail->ptr->next,tailnext,NewMeta)==tailnext){// Entering the element in the queue
                        MyTail=tail;
                        break;
                    }
            }
            else{
                NextTail->cnt++;
                NextTail->ptr=tailnext->ptr;
                CAS(&Q->Tail,tail,NextTail);// Thread helping the thread that won(in preious CAS) to change the tail to next as this node is not last anymore
            }
        }
    }
    CAS(&Q->Tail,MyTail,NewMeta);// Once thread wins and the new node is entered in the queue, thread tires to change the tail
    //if(Q->Head->ptr->next==MyTail->ptr->next)
    //    free(MyTail);
}


int dequeue(QLF *Q){
    Mptr* MyHead;
    while(1){
        int Val;
        Mptr* Head = Q->Head;
        Mptr* Tail = Q->Tail;
        Mptr* Next = Head->ptr->next;
        Mptr* NextTail=CreateMetaNode();
        if (Head==Q->Head){//Is my perception of head still the head 
            if(Head->ptr==Tail->ptr){//Only Sentinal Node in the queues
                if(Next->ptr==NULL){//is the sentinal node pointing to null
                    return -1; //Queue is empty
                }
                NextTail->cnt++;
                NextTail->ptr=Tail->ptr->next->ptr;
                CAS(&Q->Tail,Tail,NextTail);// Thread helping the thread that won(in preious CAS) to change the tail to next as this node is not last anymore
            }    
            else{
               Val = Head->ptr->val;
               Next->cnt++;
                if(CAS(&Q->Head,Head,Next)==Head){    
                    MyHead=Head;
                    break;
                }
            }
        }
    }
    /*printf("Deq Deleted Head->ptr%p\n",MyHead->ptr);
    free(MyHead->ptr);
    printf("Deq Deleted Head %p\n",MyHead);
    free(MyHead);*/
    return MyHead->ptr->next->ptr->val;
}

void *func_thread(void *id) { //Fucntion that thread will work on
    int n = (int)(Nodes / nthrds); // getting the maximum calls per thread
    //assert(n * nthr == Nodes);    
    wait_flag(&wflag, nthrds);
#ifdef CorEnq
    if (((long) id == 0)) {
            gettimeofday(&start_time, NULL);
    }
    int IdStart=n*(long)id;
    for (int i = 0; i < n; i++) {        
            enqueue(Qu,i+IdStart);
    }
#elif defined(CorDeq)
    if(SecondHalf==0){
        int IdStart=n*(long)id;
        for (int i = 0; i < n; i++) {
                enqueue(Qu,i+IdStart);//Entering Unique based on the Id value
        }
    }
    else{
        int HeadValue;
        if (((long) id == 0)) {
                gettimeofday(&start_time, NULL);
        }
        for (int ii = 0; ii < n; ii++) {
            int HeadValue=dequeue(Qu);
            if (uniqueArrDeqTest[HeadValue]==0 && Qu->Head->ptr->next!=NULL && HeadValue>=0 && HeadValue<Nodes){ 
                uniqueArrDeqTest[HeadValue]=HeadValue;
            }
            else{
                printf("Duplicate or out-of-bound Value");
                break;
            }
        }
        TotalDqs=TotalDqs+n;
        if (TotalDqs==Nodes){
                printf("\nAll Values are In-Bound and Unique in Linked List.\n");
        }
    }
#elif defined(CorEnqPlusDeq)
    int HeadValue;
    if(SecondHalf==0){
        int IdStart=n*(long)id;
        for (int i = 0; i < n/2; i++){
                enqueue(Qu,i+IdStart);//Entering Unique based on the Id value
        }
    }
    else{
        if (((long) id == 0)) {
            gettimeofday(&start_time, NULL); //Start counter for second half
        }
        for (int i = 0; i < n; i++){
            if(i%2!=0){//Dequeue when index is odd
                int HeadValue=dequeue(Qu);
                if (uniqueArrDeqTest[HeadValue]==0 && Qu->Head->ptr->next!=NULL && HeadValue>=0 && HeadValue<Nodes){ 
                    uniqueArrDeqTest[HeadValue]=HeadValue;
                }
                else{
                    printf("Duplicate or out-of-bound Value");
                    break;
                }
            }
            else{
                int IdStart=n*(long)id;
                enqueue(Qu,i+n/2+IdStart);//Entering Unique based on the Id value
            }
        }
        TotalDqs=TotalDqs+n;
        if (TotalDqs==Nodes){
                printf("\nAll Dequeue values are In-Bound and Unique in Linked List.\n");
        }
    }
#endif
    if (__sync_fetch_and_add((uint32_t *)&wflag, -1) == 1) {
        /*printf("get end time\n");*/
        gettimeofday(&end_time, NULL);
    }
    return NULL;
}

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
    //printf("Time taken: %ld.%06ld\n", (long)interval.tv_sec, (long)interval.tv_usec);
    float totalSec=interval.tv_sec+interval.tv_usec*0.000001;//microsec
    return totalSec;
}
int get_len()
{
    int cnt=0;
    node* N = Qu->Head->ptr;
    while(N!=NULL)
    {
        cnt++;
        N = N->next->ptr;
    }
    return(cnt-1);
}
int main(){
    #if defined(CorEnq)
        printf("_____Lock-Free-Queue_____\n1 Million Concurrent Enqueue Test with %d Threads\n",nthrds);
    #elif defined(CorDeq)
        printf("_____Lock-Free-Queue_____\n1 Million Concurrent Enqueue followed by 1 Million Concurrent Dequeue Test with %d Threads\n",nthrds);
    #elif defined(CorEnqPlusDeq)
        printf("_____Lock-Free-Queue_____\nHalf Million Concurrent Enqueue followed by 1 Million Concurrent Dequeue plus Enqueue Test with %d Threads\n",nthrds);
    #endif
    int len;
    Qu = (QLF*)malloc(sizeof(QLF));
    InitQueue(Qu);
    #if (defined(CorDeq) || defined(CorEnqPlusDeq))
        uniqueArrDeqTest= (int *)calloc(Nodes,sizeof(int));
    #endif
    pthread_t *thr;
    thr = calloc(sizeof(*thr), nthrds);
    for (long i = 0; i < nthrds; i++) { // creating all the threads
        if (pthread_create(&thr[i], NULL, func_thread, (void *)i) != 0) {
            perror("thread creating failed");
        }
    }
    for (long i = 0; i < nthrds; i++)
        pthread_join(thr[i], NULL);
    
#if (defined(CorDeq) || defined(CorEnqPlusDeq))
    SecondHalf=1;
    printf("Queue Length After Completeing First Enqueue Phase: %d",get_len(Qu));
    for (long i = 0; i < nthrds; i++){ // creating all the threads
        if (pthread_create(&thr[i], NULL, func_thread, (void *)i) != 0) {
            perror("thread creating failed");
        }
    }
    for (long i = 0; i < nthrds; i++)
        pthread_join(thr[i], NULL);
#endif
#if (defined(CorEnq) || defined(CorEnqPlusDeq))
    int ii;
    Mptr* Traverse=Qu->Head;
    len =get_len(Qu);
    #if defined(CorEnq)
        int *uniqueArr = (int *)calloc(Nodes,sizeof(int));
        for (ii=0;ii<len;ii++){
            Traverse=Traverse->ptr->next;
            if (uniqueArr[Traverse->ptr->val]==0 && Traverse->ptr->next!=NULL && Traverse->ptr->val>=0 && Traverse->ptr->val<Nodes){ 
                uniqueArr[Traverse->ptr->val]=Traverse->ptr->val;
            }
    #elif defined(CorEnqPlusDeq)
        int *uniqueArr = (int *)calloc(Nodes*1.5,sizeof(int));// Here value can go upto 1.5M
        for (ii=0;ii<len;ii++){
            Traverse=Traverse->ptr->next;
            if (uniqueArr[Traverse->ptr->val]==0 && Traverse->ptr->next!=NULL && Traverse->ptr->val>=0 && Traverse->ptr->val<Nodes*1.5){  
                uniqueArr[Traverse->ptr->val]=Traverse->ptr->val;
                //printf("Array reamining Value at index %d\n",Traverse->ptr->val,uniqueArr[Traverse->ptr->val]);
            }
    #endif
        else{
            printf("Duplicate or out-of-bound Value");
            break;
        }
    }
    if (ii==len){
        printf("All final values in Linked list are Unique and In-bound\n");
    }
#endif
    printf("Tail: %d\n",Qu->Tail->ptr->val);
    printf("Head: %d\n",Qu->Head->ptr->val);
    len =get_len(Qu);
    if(len==0){
    printf("Queue is empty with length zero!\n");
    }
    else{
    printf("Queue Length: %d\n",len);
    }
    float TotalTime=calc_time(&start_time, &end_time);
#if defined(CorEnq)
    printf("Throughpt of enqueues: %f Enqueues/Sec\n",Nodes/TotalTime);//Counting for million enque
#elif defined(CorDeq)
    printf("Throughpt of dequeues: %f Dequeues/Sec\n",Nodes/TotalTime);//Just counting last million deque
#elif defined(CorEnqPlusDeq)
    printf("Throughpt of enqueues plus dequeues: %f\n",Nodes/TotalTime);//Just counting last half million deque and enque each
#endif
}
