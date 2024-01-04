#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <assert.h>
#include<unistd.h>
#define Nodes 1000000
#define default_nthrds 16
#ifndef nthrds
#define nthrds default_nthrds
#endif

#define CAS(P, O, N) __sync_val_compare_and_swap((P), (O), (N))
#ifndef cpu_relax
#define cpu_relax() asm volatile("pause\n": : :"memory")
#endif
void wait_flag(volatile uint32_t *flag, uint32_t expect) {//expect is number of threads flag is updated with each call
    __sync_fetch_and_add((uint32_t *)flag, 1);
    while (*flag != expect) {// All threads looping here and cehcking when the flag has reached expected
        cpu_relax();
    }
}
static struct timeval start_time;
static struct timeval end_time;
static volatile uint32_t wflag;
static int TotalDqs=0;
static int *uniqueArrDeqTest;
static int SecondHalf=0;

struct Node{
    int val;
    struct Node* _Atomic next;
    int enqTid;
    _Atomic int deqTid;
};
typedef struct Node node;

typedef struct OperationDesc{
    long phase;
    bool pending;
    bool enqueue;
    node* myNode;
} OpDesc;

typedef struct Queue{
     node* _Atomic Tail;
     node* _Atomic Head;
     _Atomic OpDesc**  StateArray;
}QWF;

QWF* Q=NULL;

void help_en(int ndx, long phase);
void help_finish_en();
void help_de(int ndx, long phase);
void help_finish_deq();
_Atomic OpDesc* initOpDesc(long ph, bool pend, bool enq, node* N){
    _Atomic OpDesc *Op;
    Op = (OpDesc*)malloc(sizeof(OpDesc));
    Op->phase = ph;
    Op->pending = pend;
    Op->enqueue = enq;
    Op->myNode = N;
    return Op;
}

void InitQueue(){
    node* sentinal = (node*) malloc(sizeof(node));
    sentinal->next = NULL;
    sentinal->val = -1;
    sentinal->deqTid =-1;
    Q->Head = sentinal;
    Q->Tail = sentinal;
    Q->StateArray= (_Atomic OpDesc**)malloc(nthrds*sizeof(OpDesc*));
    for(int i=0; i<nthrds; i++){
        (Q->StateArray)[i]=initOpDesc(-1,false,true,NULL);
        /*(OpDesc*)malloc(sizeof(OpDesc));
        (Q->StateArray)[i]->phase=(long)-1;
        (Q->StateArray)[i]->myNode=NULL;
        (Q->StateArray)[i]->enqueue=true;
        (Q->StateArray)[i]->pending=false;*/
    }
}

void help(long phase,int Tid){   
    for(int i=0; i<nthrds; i++){
        if((Q->StateArray)[i]->pending && (Q->StateArray)[i]->phase<=phase){
            if((Q->StateArray)[i]->enqueue){
                help_en(i, phase);
            }
            else{
                help_de(i, phase);
            }
        }
    }
}

long maxPhase(){
    long maph=-1;
    for(int i=0; i<nthrds;i++){
        if((Q->StateArray)[i]->phase > maph)
            maph=(Q->StateArray)[i]->phase;
    }
    return maph;
}

bool isStillPending(int Tid, long phase){
    return (Q->StateArray)[Tid]->pending && (Q->StateArray)[Tid]->phase <=phase;
}

static volatile int32_t CurPhase =0;
static volatile int32_t LastVal =0;

node* CreateNode(int Tid, int value, long phase){
    //(node*)malloc(sizeof(node));
    node* new = (node*)malloc(sizeof(node));
    (new)->deqTid= -1;
    (new)->enqTid=Tid;
    (new)->val=value;
    (new)->next= NULL;
    (Q->StateArray)[Tid]=initOpDesc(phase,true,true,new);
    /*(Q->StateArray)[Tid]->phase=phase;
    (Q->StateArray)[Tid]->pending=true;
    (Q->StateArray)[Tid]->enqueue=true;
    (Q->StateArray)[Tid]->myNode=new;*/
    return new;
}

void help_en(int ndx, long phase){
    int broke=0;
    while(isStillPending(ndx, phase)){
        node* tail = Q->Tail;
        node* tailnext = tail->next;
        if(tail==Q->Tail){
            if (tailnext==NULL){
                if(isStillPending(ndx, phase)){
                    //if(CAS(&Q->Tail->next,tailnext,(Q->StateArray)[ndx]->myNode)==tailnext){// Changing the last node in the list
                    if(CAS(&tail->next,tailnext,(Q->StateArray)[ndx]->myNode)==tailnext){
                        help_finish_en();
                        return;
                    }
                }
            }
            else{
                /*if(Q->Tail->next==Q->Tail->next->next){
                    CAS(&Q->Tail->next->next,tailnext->next,NULL);
                }*/
                help_finish_en();
            }
        }
    }
}
void help_finish_en(){
    node* tail = Q->Tail;
    node* next = tail->next;
    if(next!=NULL){
        int Tid= next->enqTid;
        _Atomic OpDesc* CurDes = (Q->StateArray)[Tid];
        if(tail==Q->Tail && (Q->StateArray)[Tid]->myNode==next){//second condition means the cuurent new node is equals to where the tail's next is pointing
            _Atomic OpDesc* NewOpDes= initOpDesc((Q->StateArray)[Tid]->phase,false,true,next);
            /*(OpDesc*)malloc(sizeof(OpDesc));
            NewOpDes->pending=false;//Setting not pending anymore
            NewOpDes->enqueue=true;
            NewOpDes->myNode=next;//this is basically self node
            NewOpDes->phase=(Q->StateArray)[Tid]->phase;*/
            CAS(&(Q->StateArray)[Tid],CurDes,NewOpDes);
            CAS(&Q->Tail,tail,next);//Updating the last node
        }
    }
}
void help_finish_deq(){
    node* head = Q->Head;
    node* next = head->next;
    int tid = head->deqTid;
    if(tid != -1){//first dequeue step is crossed
        _Atomic OpDesc* CurDesc = (Q->StateArray)[tid];
        if(head == Q->Head && next != NULL){
            _Atomic OpDesc* NewDesc = initOpDesc((Q->StateArray)[tid]->phase,false, false, (Q->StateArray)[tid]->myNode);// No help needed hereafter
            CAS(&(Q->StateArray)[tid],CurDesc, NewDesc);
            CAS(&Q->Head,head,next); //Change the head to next node
        }
    }
}
void help_de(int ndx, long phase){
    while(isStillPending(ndx, phase)){//Checking if the the thread I am helping still needs help
        node* head = Q->Head;
        node* tail = Q->Tail;
        node* next = head->next;
        if(head == Q->Head){//is my head still the head
            if(head==tail){//Checking if the node is empty
                if(next==NULL){//Queue is empty
                    _Atomic OpDesc* CurDesc = (Q->StateArray)[ndx];
                    if(tail==Q->Tail && isStillPending(ndx,phase)){
                        _Atomic OpDesc* NewDesc = initOpDesc((Q->StateArray)[ndx]->phase,false, false, NULL);
                        CAS(&(Q->StateArray)[ndx],CurDesc,NewDesc);//Chaning to no help needed
                    }
                }
                else{//Eqnue in progress with element in list but tail not changed
                    help_finish_en();
                }
            }
            else{//Queue has elements
                _Atomic OpDesc* CurDesc = (Q->StateArray)[ndx];
                node* MyN = CurDesc->myNode;
                if(!isStillPending(ndx,phase))
                    break;
                if(head==Q->Head && MyN!=head){//Thread I am helping has some old value in its state array
                    _Atomic OpDesc* NewDesc = initOpDesc((Q->StateArray)[ndx]->phase,true, false, head);
                    if(CAS(&(Q->StateArray)[ndx],CurDesc,NewDesc)!=CurDesc){//Change the state entry of the thread I am helping to head
                        continue;
                    }
                }
                CAS(&head->deqTid,-1,ndx);//change the Deque Id in head to the node's ID which I am helping
                help_finish_deq();
            }
        }
    }
}
void Enque(int val, int Tid){
    long phase = __sync_fetch_and_add((uint32_t *)&CurPhase, 1);
    node* NewNode = CreateNode(Tid, val, phase);
    help(phase, Tid);
    help_finish_en();
}
int Deque(int TID){
    long phase = __sync_fetch_and_add((uint32_t *)&CurPhase, 1);
    (Q->StateArray)[TID]=initOpDesc(phase,true,false,NULL);
    help(phase,TID);
    help_finish_deq();
    node* RemovedNode = (Q->StateArray)[TID]->myNode;
    if (RemovedNode != NULL){
        int val=RemovedNode->next->val;
        //free(RemovedNode);
        return val;
    }
    else{
        return -1;
    }
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
    node* N = Q->Head->next;
    while(N!=NULL)
    {
        cnt++;
        N = N->next;
    }
    return(cnt);
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
            Enque(i+IdStart,(uint64_t)id);
    }
#elif defined(CorDeq)
    if(SecondHalf==0){
        for (int i = 0; i < n; i++) {
                int IdStart=n*(long)id;
                Enque(i+IdStart,(uint64_t)id);//Entering Unique based on the Id value
        }
    }
    else{
        int HeadValue;
        if (((long) id == 0)) {
                gettimeofday(&start_time, NULL);
        }
        for (int ii = 0; ii < n; ii++) {
            HeadValue=Deque((uint64_t)id);
            if (uniqueArrDeqTest[HeadValue]==0 && Q->Head!=NULL && HeadValue>=0 && HeadValue<Nodes){ 
                uniqueArrDeqTest[HeadValue]=HeadValue;
            }
            else{
                printf("Duplicate or out-of-bound Value");
                //break;
            }
        }
        TotalDqs=TotalDqs+n;
        if (TotalDqs==Nodes){
                printf("\nAll Dequeue values are In-Bound and Unique in Linked List.\n");
        }
    }
#elif defined(CorEnqPlusDeq)
    int HeadValue;
    if(SecondHalf==0){
        for (int i = 0; i < n/2; i++){
                int IdStart=n*(long)id;
                Enque(i+IdStart,(uint64_t)id);//Entering Unique based on the Id value
        }
    }
    else{
        if (((long) id == 0)) {
            gettimeofday(&start_time, NULL); //Start counter for second half
        }
        for (int i = 0; i < n; i++){
            if(i%2!=0){//Dequeue when index is odd
                int HeadValue=Deque((uint64_t)id);
                if (uniqueArrDeqTest[HeadValue]==0 && Q->Head->next!=NULL && HeadValue>=0 && HeadValue<Nodes){ 
                    uniqueArrDeqTest[HeadValue]=HeadValue;
                }
                else{
                    printf("Duplicate or out-of-bound Value");
                    break;
                }
            }
            else{
                int IdStart=n*(long)id;
                Enque(i+IdStart,(uint64_t)id);//Entering Unique based on the Id value
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

int main(){
    #if defined(CorEnq)
        printf("_____Wait-Free-Queue_____\n1 Million Concurrent Enqueue Test with %d Threads\n",nthrds);
    #elif defined(CorDeq)
        printf("_____Wait-Free-Queue_____\n1 Million Concurrent Enqueue followed by 1 Million Concurrent Dequeue Test with %d Threads\n",nthrds);
    #elif defined(CorEnqPlusDeq)
        printf("_____Wait-Free-Queue_____\nHalf Million Concurrent Enqueue followed by 1 Million Concurrent Dequeue plus Enqueue Test with %d Threads\n",nthrds);
    #endif
    int len;
    Q = (QWF*)malloc(sizeof(QWF));
    InitQueue(Q);
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
    printf("Queue Length After Completeing First Enqueue Phase: %d",get_len(Q));
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
    node* Traverse=Q->Head;
    len =get_len(Q);
    #if defined(CorEnq)
        int *uniqueArr = (int *)calloc(Nodes,sizeof(int));
        for (ii=0;ii<len;ii++){
            Traverse=Traverse->next;
            if (uniqueArr[Traverse->val]==0 && Traverse!=NULL && Traverse->val>=0 && Traverse->val<Nodes){ 
                uniqueArr[Traverse->val]=Traverse->val;
            }
    #elif defined(CorEnqPlusDeq)
        int *uniqueArr = (int *)calloc(Nodes*1.5,sizeof(int));// Here value can go upto 1.5M
        for (ii=0;ii<len;ii++){
            Traverse=Traverse->next;
            if (uniqueArr[Traverse->val]==0 && Traverse!=NULL && Traverse->val>=0 && Traverse->val<Nodes*1.5){  
                uniqueArr[Traverse->val]=Traverse->val;
                //printf("Array reamining Value at index %d\n",Traverse->val,uniqueArr[Traverse->val]);
            }
    #endif
        else{
            printf("Duplicate or out-of-bound Value");
            break;
        }
    }
    if (ii==len){
        printf("All final values in Linked list are Unique and in-bound\n");
    }
#endif
    printf("Tail: %d\n",Q->Tail->val);
    printf("Head: %d\n",Q->Head->val);
    len =get_len(Q);
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