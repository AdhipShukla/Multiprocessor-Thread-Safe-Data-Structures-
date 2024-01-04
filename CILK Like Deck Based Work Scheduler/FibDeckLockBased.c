#include<stdio.h>
#include<stdlib.h>
#include<sys/time.h>
#include<stdint.h>
#include<pthread.h>
#include<assert.h>
#include"spinlock-TTAS-Backoff.h"
//#define Nthrds 16
#ifndef cpu_relax
#define cpu_relax() asm volatile("pause\n": : :"memory");
#endif
static volatile uint32_t wflag;
static struct timeval StartTime, EndTime;
spinlock slLeft, slRight, slLastNode, slSize;
static int Nthrds = 0;
struct Node{
    struct Node *left;
    struct Node *right;
    int val;
    int MyFibo;
    int Done;
};
typedef struct Node node;

int Fib(node *Job, int MyId);
node *InitNode;

typedef struct Anchor{//This is essentially the Deck
    node *LMost; //set to null initially
    node *RMost; //set to null initially
    long size;
}anchor;

//anchor *Deck[Nthrds]; //Set the initial value by calling Set Deck in the main function
anchor **Deck;
anchor *SetDeck(node* ptrL,node* ptrR, long S){
    anchor *NewDeck= (anchor*)malloc(sizeof(anchor));
    NewDeck->LMost=(node*)malloc(sizeof(node));
    NewDeck->LMost=ptrL;
    NewDeck->RMost=(node*)malloc(sizeof(node));
    NewDeck->RMost=ptrR;
    NewDeck->size=S;
    return NewDeck;
}

node *CreateNode(int Value){
    node *NewNode = (node*)malloc(sizeof(node));
    NewNode->val = Value;
    NewNode->Done = 0;
    NewNode->MyFibo = 0;
    NewNode->left = (node*)malloc(sizeof(node));
    NewNode->right = (node*)malloc(sizeof(node));
    return NewNode;
}
void PushLeft(node *NewNode, int Id){
    spin_lock(&slLeft);
    int OtherUnlocked=1;
    if(Deck[Id]->size<=1){
        spin_unlock(&slLeft);//Let pull work from left side
        spin_lock(&slLastNode);//To prevent deadlocking
        spin_lock(&slRight);//To prevent concurrent pop and push right
        spin_lock(&slLeft);//To prevent concurrent PopLeft
        if(Deck[Id]->LMost==NULL){//The queue is empty
            Deck[Id]->LMost=NewNode;
            Deck[Id]->RMost=NewNode;
            //__sync_fetch_and_add((long*)&Deck[Id]->size,1);//Increment the size
            spin_lock(&slSize);
            Deck[Id]->size++;
            spin_unlock(&slSize);
            spin_unlock(&slLastNode);
            spin_unlock(&slRight);
            spin_unlock(&slLeft);
            return;
        }
        OtherUnlocked=0;
        if(Deck[Id]->size>1){
            spin_unlock(&slRight);
            spin_unlock(&slLastNode);
            OtherUnlocked=1;
        }
    }
    NewNode->right=Deck[Id]->LMost;
    Deck[Id]->LMost->left=NewNode;
    Deck[Id]->LMost=NewNode;
    //__sync_fetch_and_add((long*)&Deck[Id]->size,1);//Increment the size
    spin_lock(&slSize);
    Deck[Id]->size++;
    spin_unlock(&slSize);
    if(OtherUnlocked==0){
        spin_unlock(&slRight);
        spin_unlock(&slLastNode);
    }
    spin_unlock(&slLeft);
    return;
}
void PushRight(node *NewNode, int Id){
    spin_lock(&slRight);
    int OtherUnlocked=1;
    if(Deck[Id]->size<=1){
        spin_unlock(&slRight);//Let pull work from right side
        spin_lock(&slLastNode);//To prevent deadlocking
        spin_lock(&slLeft);//To prevent concurrent pop and push left
        spin_lock(&slRight);//To prevent further concurrent Popright
        if(Deck[Id]->RMost==NULL){//The queue is empty
            Deck[Id]->RMost=NewNode;
            Deck[Id]->LMost=NewNode;
            //__sync_fetch_and_add((long*)&Deck[Id]->size,(long)1);//Increment the size
            spin_lock(&slSize);
            Deck[Id]->size++;
            spin_unlock(&slSize);
            spin_unlock(&slLastNode);
            spin_unlock(&slLeft);
            spin_unlock(&slRight);
            return;
        }
        OtherUnlocked=0;
        if(Deck[Id]->size>1){
            spin_unlock(&slLeft);
            spin_unlock(&slLastNode);
            OtherUnlocked=1;
        }
    }
    NewNode->left=Deck[Id]->RMost;
    Deck[Id]->RMost->right=NewNode;
    Deck[Id]->RMost=NewNode;
    //__sync_fetch_and_add((long*)&Deck[Id]->size,(long)1);//Increment the size
    spin_lock(&slSize);
    Deck[Id]->size++;
    spin_unlock(&slSize);
    if(OtherUnlocked==0){
        spin_unlock(&slLeft);
        spin_unlock(&slLastNode);
    }
    spin_unlock(&slRight);
    return;
}
node *PopLeft(int Id){
    spin_lock(&slLeft);
    /*if(Deck->size==0){//Deck is empty
        spin_unlock(&slLeft);
        return -1;
    }*/
    int OtherUnlocked=1;
    if(Deck[Id]->size<=2){
        spin_unlock(&slLeft);//Let the push work of left side
        spin_lock(&slLastNode);//To prevent deadlocking
        spin_lock(&slRight);//To prevent concurrent pop and push right
        spin_lock(&slLeft);//To prevent concurrent pushLeft
        OtherUnlocked=0;
        if(Deck[Id]->size>2){
            spin_unlock(&slRight);
            spin_unlock(&slLastNode);
            OtherUnlocked=1;
        }
    }
    if(Deck[Id]->size==0){//Deck is empty
        spin_unlock(&slLeft);
        if(OtherUnlocked==0){
            spin_unlock(&slRight);
            spin_unlock(&slLastNode);
        }
        return NULL;
    }
    node* temp=Deck[Id]->LMost;
    Deck[Id]->LMost=Deck[Id]->LMost->right;
    //Deck[Id]->LMost->left=NULL;//Breaking the link with removed node
    //__sync_fetch_and_add((long*)&Deck[Id]->size,(long)-1);//Decrement the size
    spin_lock(&slSize);
    Deck[Id]->size--;
    spin_unlock(&slSize);
    if(Deck[Id]->size==0){
        Deck[Id]->LMost=NULL;
        Deck[Id]->RMost=NULL;
    }
    //int val = temp->val;
    //free(temp);
    if(OtherUnlocked==0){
        spin_unlock(&slRight);
        spin_unlock(&slLastNode);
    }
    spin_unlock(&slLeft);
    return temp;
}
node *PopRight(int Id){
    spin_lock(&slRight);
    /*if(Deck[Id]->size==0){//Deck is empty
        spin_unlock(&slRight);
        return NULL;
    }*/
    int OtherUnlocked=1;
    if(Deck[Id]->size<=2){
        spin_unlock(&slRight);//Let the push work of left side
        spin_lock(&slLastNode);//To prevent deadlocking
        spin_lock(&slLeft);//To prevent concurrent pushLeft
        spin_lock(&slRight);//To prevent concurrent pop and push right
        OtherUnlocked=0;
        if(Deck[Id]->size>2){
            spin_unlock(&slLeft);
            spin_unlock(&slLastNode);
            OtherUnlocked=1;
        }
    }
    if(Deck[Id]->size==0){//Deck is empty
        spin_unlock(&slRight);
        if(OtherUnlocked==0){
            spin_unlock(&slLeft);
            spin_unlock(&slLastNode);
        }
        return NULL;
    }
    node* temp=Deck[Id]->RMost;
    Deck[Id]->RMost=Deck[Id]->RMost->left;
    //Deck[Id]->RMost->right=NULL;//Breaking the link with removed node
    //__sync_fetch_and_add((long*)&Deck[Id]->size,(long)-1);//Decrement the size 
    spin_lock(&slSize);
    Deck[Id]->size--;
    spin_unlock(&slSize);
    if(Deck[Id]->size==0){
        Deck[Id]->LMost=NULL;
        Deck[Id]->RMost=NULL;
    }
    //int val = temp->val;
    //free(temp);
    if(OtherUnlocked==0){
        spin_unlock(&slLeft);
        spin_unlock(&slLastNode);
    }
    spin_unlock(&slRight);
    return temp;
}

void wait_flags(volatile uint32_t *flag, uint32_t Id){
    __sync_fetch_and_add((uint32_t *)flag,1);
    while(*flag!=Id){//Every thread spins till the value of flag is not equal to Nthreads
        cpu_relax();
    }
}
void Sync(node *SpawnedNode, int MyId){
    while(SpawnedNode->Done==0){//Threads infinitely finding their first work till the job is not done completely
        node *MyNextJob;
        for(int i=0; i<Nthrds; i++){
            if(Deck[i]->RMost!=NULL){
                if(MyId==i){
                    MyNextJob=PopRight(i);
                    if (MyNextJob!=NULL){
                        Fib(MyNextJob, MyId);
                        break;
                    }    
                }
                else{
                    MyNextJob=PopLeft(i);    
                    if (MyNextJob!=NULL){
                        Fib(MyNextJob, MyId);
                        break;
                    }
                }
            }
            /*if(Deck[i]->size>0 && MyId==i){
                MyNextJob=PopRight(i);
                if (MyNextJob!=NULL){
                    Fib(MyNextJob, MyId);
                    break;
                }    
            }
            else if(Deck[i]->size>0){
                MyNextJob=PopLeft(i);    
                if (MyNextJob!=NULL){
                    Fib(MyNextJob, MyId);
                    break;
                }
            }*/
        }
    }
}
node *Spawn(int PushOnDeck, int MyId){
    node *NNode= CreateNode(PushOnDeck);
    PushRight(NNode, MyId);
    return NNode;
}
int Fib(node *Job, int MyId){
    if (Job->val<2){
        Job->MyFibo=Job->val;
        Job->Done=1;
        return Job->MyFibo;
    }
    else{
        int Y;
        node *XNode= Spawn(Job->val-1, MyId);//Put the elements on the deck. Spawn will return the newNode created for this element
        //XNode->N;
        Y=Fib(CreateNode(Job->val-2), MyId);//Execute by self
        Sync(XNode, MyId);//Look for other tasks that are remaining in the decks
        Job->MyFibo= XNode->MyFibo+Y;
        Job->Done=1;
        return Job->MyFibo;//
    }
}
void *Thread_Func(void* Idx){
    wait_flags(&wflag,Nthrds);
    if(Idx==0){
        gettimeofday(&StartTime, NULL);
    }
    node* MyFirstJob;
    while(!InitNode->Done){//Threads infinitely finding their first work till the job is not done completely
        for(int i=0; i<Nthrds; i++){
            if(Deck[i]->RMost!=NULL){//Check if empty or not
                if((uint64_t)Idx==i){
                    MyFirstJob=PopRight(i);
                    if (MyFirstJob!=NULL)
                        break;
                }
                else{
                    MyFirstJob=PopLeft(i);
                    if (MyFirstJob!=NULL)
                        break;
                }
            }
            /*if(Deck[i]->size>0 && (uint64_t)Idx==i){
                MyFirstJob=PopRight(i);
                if (MyFirstJob!=NULL)
                    break;
            }
            else if(Deck[i]->size>0){
                MyFirstJob=PopLeft(i);
                if (MyFirstJob!=NULL)
                    break;
            }*/
        }
            if (MyFirstJob!=NULL) //If the work is found then break this loop
                    break;
    }
    if(!InitNode->Done)
        Fib(MyFirstJob, (uint64_t)Idx);
    if(__sync_fetch_and_add((uint32_t*)&wflag,-1)==1){
        gettimeofday(&EndTime, NULL);
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
    printf("Time taken: %ld.%06ld\n", (long)interval.tv_sec, (long)interval.tv_usec);
    float totalSec=interval.tv_sec+interval.tv_usec*0.000001;//microsec
    return totalSec;
}

int main(int argc, const char *argv[]){
    if (argc != 2) {
        printf("Usage: %s <num of threads>\n", argv[0]);
        exit(1);
    }
    Nthrds = atoi(argv[1]); // Getting the number of threads
    Deck=(anchor **)malloc(sizeof(anchor*)*Nthrds); 
    for (int i=0; i<Nthrds; i++){
        Deck[i]= SetDeck(NULL,NULL,0);//Initialzing the Deque
    }
    int FibNum;
    while(1){
        printf("Enter a number for which you are calculating Fibonacci: \n");
        scanf("%d",&FibNum);
        if(FibNum<=30 && FibNum>0){
            break;   
        }
        else{
            printf("Number out of range can lead to memory issues!\n");
        }
    }
    InitNode= CreateNode(FibNum);
    Deck[0]->LMost=InitNode;
    Deck[0]->RMost=InitNode;
    Deck[0]->size=(long)1;
    pthread_t *thr;
    thr= calloc(Nthrds,sizeof(*thr));
    for(long i=0; i<Nthrds; i++){
       if(pthread_create(&thr[i],NULL,Thread_Func,(void *)i)!=0)
            perror("Thread creation failed");    
    }
    for(long i=0; i<Nthrds;i++){
        pthread_join(thr[i],NULL);
    }
    printf("Fibonacci of %d is %d\n",InitNode->val,InitNode->MyFibo);
    float TotalTime=calc_time(&StartTime, &EndTime);
    
}