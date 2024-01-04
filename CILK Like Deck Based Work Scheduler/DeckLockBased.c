#include<stdio.h>
#include<stdlib.h>
#include<sys/time.h>
#include<stdint.h>
#include <pthread.h>
#include"spinlock-TTAS-Backoff.h"
#define Iter 1600000
#ifndef cpu_relax
#define cpu_relax() asm volatile("pause\n": : :"memory");
#endif
//typedef enum Status{Stable, Rpush, Lpush} status;
static volatile uint32_t wflag;
static struct timeval StartTime, EndTime;
spinlock slLeft, slRight, slLastNode;
static int Nthrds = 0;
struct Node{
    struct Node* left;
    struct Node* right;
    int val;
};
typedef struct Node node;

typedef struct Anchor{//This is essentially the Deck
    node *LMost; //set to null initially
    node *RMost; //set to null initially
    long size;
}anchor;

anchor *Deck; //Set the initial value by calling Set Deck in the main function

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
    node* NewNode = (node*)malloc(sizeof(node));
    NewNode->val = Value;
    NewNode->left = (node*)malloc(sizeof(node));
    NewNode->right = (node*)malloc(sizeof(node));
    return NewNode;
}
void PushLeft(node *NewNode){
    spin_lock(&slLeft);
    int OtherUnlocked=1;
    if(Deck->size<=1){
        spin_unlock(&slLeft);//Let pull work from left side
        spin_lock(&slLastNode);//To prevent deadlocking
        spin_lock(&slRight);//To prevent concurrent pop and push right
        spin_lock(&slLeft);//To prevent concurrent PopLeft
        if(Deck->LMost==NULL){//The queue is empty
            Deck->LMost=NewNode;
            Deck->RMost=NewNode;
            __sync_fetch_and_add((long*)&Deck->size,1);//Increment the size
            spin_unlock(&slLastNode);
            spin_unlock(&slRight);
            spin_unlock(&slLeft);
            return;
        }
        OtherUnlocked=0;
        if(Deck->size>1){
            spin_unlock(&slRight);
            spin_unlock(&slLastNode);
            OtherUnlocked=1;
        }
    }
    NewNode->right=Deck->LMost;
    Deck->LMost->left=NewNode;
    Deck->LMost=NewNode;
    __sync_fetch_and_add((long*)&Deck->size,1);//Increment the size
    if(OtherUnlocked==0){
        spin_unlock(&slRight);
        spin_unlock(&slLastNode);
    }
    spin_unlock(&slLeft);
    return;
}
void PushRight(node *NewNode){
    spin_lock(&slRight);
    int OtherUnlocked=1;
    if(Deck->size<=1){
        spin_unlock(&slRight);//Let pull work from right side
        spin_lock(&slLastNode);//To prevent deadlocking
        spin_lock(&slLeft);//To prevent concurrent pop and push left
        spin_lock(&slRight);//To prevent further concurrent Popright
        if(Deck->RMost==NULL){//The queue is empty
            Deck->RMost=NewNode;
            Deck->LMost=NewNode;
            __sync_fetch_and_add((long*)&Deck->size,1);//Increment the size
            spin_unlock(&slLastNode);
            spin_unlock(&slLeft);
            spin_unlock(&slRight);
            return;
        }
        OtherUnlocked=0;
        if(Deck->size>1){
            spin_unlock(&slLeft);
            spin_unlock(&slLastNode);
            OtherUnlocked=1;
        }
    }
    NewNode->left=Deck->RMost;
    Deck->RMost->right=NewNode;
    Deck->RMost=NewNode;
    __sync_fetch_and_add((long*)&Deck->size,1);//Increment the size
    if(OtherUnlocked==0){
        spin_unlock(&slLeft);
        spin_unlock(&slLastNode);
    }
    spin_unlock(&slRight);
    return;
}
int PopLeft(){
    spin_lock(&slLeft);
    /*if(Deck->size==0){//Deck is empty
        spin_unlock(&slLeft);
        return -1;
    }*/
    int OtherUnlocked=1;
    if(Deck->size<=2){
        spin_unlock(&slLeft);//Let the push work of left side
        spin_lock(&slLastNode);//To prevent deadlocking
        spin_lock(&slRight);//To prevent concurrent pop and push right
        spin_lock(&slLeft);//To prevent concurrent pushLeft
        OtherUnlocked=0;
        if(Deck->size>2){
            spin_unlock(&slRight);
            spin_unlock(&slLastNode);
            OtherUnlocked=1;
        }
    }
    if(Deck->size==0){//Deck is empty
        spin_unlock(&slLeft);
        if(OtherUnlocked==0){
        spin_unlock(&slRight);
        spin_unlock(&slLastNode);
        }
        return -1;
    }
    node* temp=Deck->LMost;
    Deck->LMost=Deck->LMost->right;
    __sync_fetch_and_add((long*)&Deck->size,-1);//Decrement the size
    if(Deck->size==0){
        Deck->LMost=NULL;
        Deck->RMost=NULL;
    }
    int val = temp->val;
    free(temp);
    if(OtherUnlocked==0){
        spin_unlock(&slRight);
        spin_unlock(&slLastNode);
    }
    spin_unlock(&slLeft);
    return val;
}
int PopRight(){
    spin_lock(&slRight);
    if(Deck->size==0){//Deck is empty
        spin_unlock(&slRight);
        return -1;
    }
    int OtherUnlocked=1;
    if(Deck->size<=2){
        spin_unlock(&slRight);//Let the push work of left side
        spin_lock(&slLastNode);//To prevent deadlocking
        spin_lock(&slLeft);//To prevent concurrent pushLeft
        spin_lock(&slRight);//To prevent concurrent pop and push right
        OtherUnlocked=0;
        if(Deck->size>2){
            spin_unlock(&slLeft);
            spin_unlock(&slLastNode);
            OtherUnlocked=1;
        }
    }
    if(Deck->size==0){//Deck is empty
        spin_unlock(&slRight);
        if(OtherUnlocked==0){
        spin_unlock(&slLeft);
        spin_unlock(&slLastNode);
        }
        return -1;
    }
    node* temp=Deck->RMost;
    Deck->RMost=Deck->RMost->left;
    __sync_fetch_and_add((long*)&Deck->size,-1);//Decrement the size 
    if(Deck->size==0){
        Deck->LMost=NULL;
        Deck->RMost=NULL;
    }
    int val = temp->val;
    free(temp);
    if(OtherUnlocked==0){
        spin_unlock(&slLeft);
        spin_unlock(&slLastNode);
    }
    spin_unlock(&slRight);
    return val;
}
int get_len(){
    int cnt =0;
    node *CurNode=Deck->LMost;
    while(Deck->LMost!=NULL){
        printf("%d\t",CurNode->val);
        cnt++;
        if(CurNode==Deck->RMost){
            return cnt;
        }
        CurNode=CurNode->right;
    }
    return 0;
}

void wait_flags(volatile uint32_t *flag, uint32_t Id){
    __sync_fetch_and_add((uint32_t *)flag,1);
    while(*flag!=Id){//Every thread spins till the value of flag is not equal to Nthreads
        cpu_relax();
    }
}

void *Thread_Func(void* Idx){
    int n = (int)Iter/Nthrds;
    wait_flags(&wflag,Nthrds);
    if(Idx==0){
        gettimeofday(&StartTime, NULL);
    }
    for(long i;i<n;i++){
        int random=rand();
        if(random%4==1){
            node* NewNode = CreateNode(i+((long)Idx*n));
            PushRight(NewNode);
        }
        else if(random%4==2){
            node* NewNode = CreateNode(i+((long)Idx*n));
            PushLeft(NewNode);
        }
        else if(random%4==3){
            PopLeft();
        }
        else{
            PopRight();
        }

    }
    if(__sync_fetch_and_add((uint32_t*)&wflag,-1)==1){
        gettimeofday(&EndTime, NULL);
    }
    return NULL;
}

int main(int argc, const char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s <num of threads>\n", argv[0]);
        exit(1);
    }
    Nthrds = atoi(argv[1]); // Getting the number of threads
    Deck= SetDeck(NULL,NULL,0);//Initialzing the Deque
    pthread_t *thr;
    thr= calloc(Nthrds,sizeof(*thr));
    for(long i=0; i<Nthrds; i++){
       if(pthread_create(&thr[i],NULL,Thread_Func,(void *)i)!=0)
            perror("Thread creation failed");    
    }
    for(long i=0; i<Nthrds;i++){
        pthread_join(thr[i],NULL);
    }
    node *CurNode=Deck->LMost;
    printf("\n");
    int Len=get_len();
    printf("\nThe length of Deck is %d = %d",Len,Deck->size);
    int *UniqueArr=(int*)calloc(Iter,sizeof(int));
    node* Traverse=Deck->LMost;
    int i;
    for(i=0; i<Len; i++){
        if(UniqueArr[Traverse->val]!=0){
            printf("Duplicate Element!");
            break;
        }
        UniqueArr[Traverse->val]=Traverse->val;
        Traverse= Traverse->right;
    }
    if(i==Len){
        printf("\nNo duplicate item found in random push and pop test with 1600000 Nodes\n");
    }
}
