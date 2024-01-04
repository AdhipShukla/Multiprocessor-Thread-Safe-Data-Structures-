#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<sys/time.h>
#include<stdint.h>
#include<unistd.h>
#include<assert.h>
#define CAS(P, O, N) __sync_val_compare_and_swap((P),(O),(N))
//#define Nthrds 16
#ifndef cpu_relax
#define cpu_relax() asm volatile("pause\n": : :"memory");
#endif
typedef enum Status{Stable, Rpush, Lpush} status;
static volatile uint32_t wflag;
static struct timeval StartTime, EndTime;
static int Nthrds = 0;
struct Node{
    struct Node* left;
    struct Node* right;
    int val;
    int MyFibo;
    int Done;
};
typedef struct Node node;

int Fib(node *Job, int MyId);
node *InitNode;

typedef struct Anchor{
    node *LMost; //set to null initially
    node *RMost; //set to null initially
    status sta;
}anchor;

//anchor *Deck[Nthrds]; //Set the initial value by calling Set Deck in the main function
anchor **Deck;
anchor *SetDeck(node* ptrL,node* ptrR, status S){
    anchor *NewDeck= (anchor*)malloc(sizeof(anchor));
    NewDeck->LMost=(node*)malloc(sizeof(node));
    NewDeck->LMost=ptrL;
    NewDeck->RMost=(node*)malloc(sizeof(node));
    NewDeck->RMost=ptrR;
    NewDeck->sta=S;
    return NewDeck;
}

node *CreateNode(int Value){
    node* NewNode = (node*)malloc(sizeof(node));
    NewNode->val = Value;
    NewNode->Done = 0;
    NewNode->MyFibo = 0;
    NewNode->left = (node*)malloc(sizeof(node));
    NewNode->right = (node*)malloc(sizeof(node));
    return NewNode;
}

void StablizeRight(anchor *De, int Id){
    if(Deck[Id]!=De) return;
    node *PrevRend = De->RMost->left;
    node* PrevRendRight=PrevRend->right;
    if(PrevRendRight!=De->RMost){
        if(Deck[Id]!=De) return;
        if(CAS(&PrevRend->right,PrevRendRight,De->RMost)==PrevRendRight) return;
    }
    CAS(&Deck[Id],De,SetDeck(De->LMost,De->RMost,Stable));//Make the Deck Stable
}

void StablizeLeft(anchor *De, int Id){
    if(Deck[Id]!=De) return;
    node *PrevLend = De->LMost->right;
    //if (De){}
    node* PrevLendLeft=PrevLend->left;
    if(PrevLendLeft!=De->LMost){
        if(Deck[Id]!=De) return;
        if(CAS(&PrevLend->left,PrevLendLeft,De->LMost)==PrevLendLeft) return;
    }
    CAS(&Deck[Id],De,SetDeck(De->LMost,De->RMost,Stable));//Make the Deck Stable
}

void Stablize(anchor *De, int Id){
    if(De->sta==Rpush){
        StablizeRight(De, Id);
    }
    else if(De->sta==Lpush){
        StablizeLeft(De, Id);
    }
}

//Push Right Performed by the same Process
void PushRight(node* NewNode, int Id){
    while(1){
        anchor *MyDeck= Deck[Id];
        node* Lend= MyDeck->LMost;
        node* Rend= MyDeck->RMost;
        status S= MyDeck->sta;
        if (Rend==NULL){//Deck is empty
            if(CAS(&Deck[Id],MyDeck,SetDeck(NewNode,NewNode,S))==MyDeck){
                return;
            }
        }
        else if(S==Stable){ //The Deck is stable and has one or more elements
            NewNode->left=Rend;
            anchor *NewDeck= SetDeck(Lend,NewNode,Rpush);
            if(CAS(&Deck[Id],MyDeck,NewDeck)==MyDeck){
                StablizeRight(NewDeck, Id);//Make the last right most to point to new right most then make the Deck Stable
                return;
            }
        }
        else{
            Stablize(MyDeck, Id);// Help the Deck be stable
        }
    }
}

//Push Left May not be used
void PushLeft(node* NewNode, int Id){
    while(1){
        anchor *MyDeck= Deck[Id];
        node* Lend= MyDeck->LMost;
        node* Rend= MyDeck->RMost;
        status S= MyDeck->sta;
        if (Lend==NULL){//Deque is empty
            if(CAS(&Deck[Id],MyDeck,SetDeck(NewNode,NewNode,S))==MyDeck){
                return;
            }
        }
        else if(S==Stable){ //The Deck is stable and has one or more elements
            NewNode->right=Lend;
            anchor *NewDeck= SetDeck(NewNode,Rend,Lpush);
            if(CAS(&Deck[Id],MyDeck,NewDeck)==MyDeck){
                StablizeLeft(NewDeck, Id);//Make the last left most to point to new left most then make the Deck Stable
                return;
            }
        }
        else{
            Stablize(MyDeck, Id);// Help the Deck be stable
        }
    }
}

//Only local pop can occur from the right
node *PopRight(int Id){
    anchor *MyFinalDeck;
    while(1){
        anchor *MyDeck= Deck[Id];
        node* Lend= MyDeck->LMost;
        node* Rend= MyDeck->RMost;
        status S= MyDeck->sta;
        if(Rend==NULL) return NULL; //Empty deque
        if(Rend==Lend){
            if(CAS(&Deck[Id], MyDeck, SetDeck(NULL,NULL,Stable))==MyDeck){
                MyFinalDeck=MyDeck;
                break;
            }
        }
        else if(S==Stable){
            if (Deck[Id] != MyDeck) continue;
            node *NewRend = Rend->left;
            if(CAS(&Deck[Id], MyDeck,SetDeck(Lend,NewRend,Stable))==MyDeck){
                MyFinalDeck=MyDeck;
                break;
            }
        }
        else{
            Stablize(MyDeck, Id);
        }
    }
    int data = MyFinalDeck->RMost->val;
    //free(MyFinalDeck);
    return MyFinalDeck->RMost;
}

//Only remote pop can occur from the left
node *PopLeft(int Id){
    anchor *MyFinalDeck;
    while(1){
        anchor *MyDeck= Deck[Id];
        node* Lend= MyDeck->LMost;
        node* Rend= MyDeck->RMost;
        status S= MyDeck->sta;
        if(Lend==NULL) return NULL; //Empty deque
        if(Rend==Lend){
            if(CAS(&Deck[Id], MyDeck, SetDeck(NULL,NULL,Stable))==MyDeck){
                MyFinalDeck=MyDeck;
                break;
            }
        }
        else if(S==Stable){
            if (Deck[Id] != MyDeck) continue;
            node *NewLend = Lend->right;
            if(CAS(&Deck[Id], MyDeck,SetDeck(NewLend,Rend,Stable))==MyDeck){
                MyFinalDeck=MyDeck;
                break;
            }
        }
        else{
            Stablize(MyDeck, Id);
        }
    }
    int data = MyFinalDeck->LMost->val;
    //free(MyFinalDeck);
    return MyFinalDeck->LMost;
}

void wait_flags(volatile uint32_t *flag, uint32_t Id){
    __sync_fetch_and_add((uint32_t *)flag,1);
    while(*flag!=Id){
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
    //Considering right side local will operate and left side remote will operate
    //int n = (int)Iter/Nthrds;
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
            printf("Number out of range(>30) can lead to memory issues!\n");
        }
    }
    InitNode= CreateNode(FibNum);
    Deck[0]->LMost=InitNode;
    Deck[0]->RMost=InitNode;
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