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
    int MyLow;
    int MyHigh;
    int Done;
};
typedef struct Node node;
void quickSort(node* Job, int Id);
node *InitNode;
int *Arr;

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

node *CreateNode(int H, int L){
    node* NewNode = (node*)malloc(sizeof(node));
    NewNode->MyHigh = H;
    NewNode->MyLow = L;
    NewNode->Done = 0;
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
                        quickSort(MyNextJob, MyId);
                        break;
                    }    
                }
                else{
                    MyNextJob=PopLeft(i);    
                    if (MyNextJob!=NULL){
                        quickSort(MyNextJob, MyId);
                        break;
                    }
                }
            }
        }
    }
}

node *Spawn(int PushOnDeckLow, int PushOnDeckHigh, int MyId){
    node *NNode= CreateNode(PushOnDeckHigh, PushOnDeckLow);
    PushRight(NNode, MyId);
    return NNode;
}

int PartitionArr(int *Arr, int L, int H){
    int i=L-1;
    int temp;
    for(int j=L; j< H;j++){
        if(Arr[j]<Arr[H]){
            i++;
            temp=Arr[j];
            Arr[j]=Arr[i];
            Arr[i]=temp;
        }
    }
    temp=Arr[i+1];
    Arr[i+1]=Arr[H];
    Arr[H]=temp;
    return i+1;
}

void quickSort(node* Job, int Id){
    int High= Job->MyHigh;
    int Low= Job->MyLow;
    if(High<Low){
        Job->Done=1;
        return;
    }
    else{
        int PivotFinal= PartitionArr(Arr, Low, High);
        node *XNode= Spawn(Low, PivotFinal-1, Id);
        quickSort(CreateNode(High, PivotFinal+1), Id);
        Sync(XNode, Id);
        Job->Done=1;
        return;
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
    if(!InitNode->Done)//Work is not done so call QSort
        quickSort(MyFirstJob, (uint64_t)Idx);
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

void printArray(int array[], int size) {
  for (int i = 0; i < size; ++i) {
    printf("%d  ", array[i]);
  }
  printf("\n");
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
    int Len;
    while(1){
        printf("Enter the length of randomly ordered array: \n");
        scanf("%d",&Len);
        if(Len<=500000 && Len>0){
            break;   
        }
        else{
            printf("Number out of range(>500000) can lead to memory issues!\n");
        }
    }
    Arr= (int *)malloc(sizeof(int)*Len);
    for(int i = 0; i < Len; i++) { 
        Arr[i] = (rand() % (Len*2 + 1)); 
    }
    //printf("Unsorted Array\n");
    //printArray(Arr, Len);

    InitNode= CreateNode(Len-1,0);
    Deck[0]->LMost=InitNode;
    Deck[0]->RMost=InitNode;
    pthread_t *thr;
    thr= calloc(Nthrds,sizeof(*thr));
    for(long i=0; i<Nthrds; i++){
       if(pthread_create(&thr[i],NULL,Thread_Func,(void *)i)!=0)
            perror("Thread creation failed");    
    }
    for(long i=0; i<Nthrds; i++){
        pthread_join(thr[i],NULL);
    }
    //printf("Sorted array in ascending order: \n");
    //printArray(Arr, Len);
    float TotalTime=calc_time(&StartTime, &EndTime);
    int i;
    for(i=0; i<Len-1; i++){
        if (Arr[i]>Arr[i+1]){
            printf("Not sorted!!\n");
            break;
        }
    }
    if(i==Len-1){
        printf("Totally Sorted!\n");
    }
    return 0;
}