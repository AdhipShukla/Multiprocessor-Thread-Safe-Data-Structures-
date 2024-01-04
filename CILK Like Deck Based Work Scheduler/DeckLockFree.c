#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<sys/time.h>
#include<stdint.h>
#include<unistd.h>
#include<assert.h>
#define CAS(P, O, N) __sync_val_compare_and_swap((P),(O),(N))
#define Iter 1600000
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
};
typedef struct Node node;

typedef struct Anchor{
    node *LMost; //set to null initially
    node *RMost; //set to null initially
    status sta;
}anchor;

anchor *Deck; //Set the initial value by calling Set Deck in the main function

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
    NewNode->left = (node*)malloc(sizeof(node));
    NewNode->right = (node*)malloc(sizeof(node));
    return NewNode;
}

void StablizeRight(anchor *De){
    if(Deck!=De) return;
    node *PrevRend = De->RMost->left;
    //if (De){}
    node* PrevRendRight=PrevRend->right;
    if(PrevRendRight!=De->RMost){
        if(Deck!=De) return;
        if(CAS(&PrevRend->right,PrevRendRight,De->RMost)==PrevRendRight) return;
    }
    CAS(&Deck,De,SetDeck(De->LMost,De->RMost,Stable));//Make the Deck Stable
}

void StablizeLeft(anchor *De){
    if(Deck!=De) return;
    node *PrevLend = De->LMost->right;
    //if (De){}
    node* PrevLendLeft=PrevLend->left;
    if(PrevLendLeft!=De->LMost){
        if(Deck!=De) return;
        if(CAS(&PrevLend->left,PrevLendLeft,De->LMost)==PrevLendLeft) return;
    }
    CAS(&Deck,De,SetDeck(De->LMost,De->RMost,Stable));//Make the Deck Stable
}

void Stablize(anchor *De){
    if(De->sta==Rpush){
        StablizeRight(De);
    }
    else if(De->sta==Lpush){
        StablizeLeft(De);
    }
}

//Push Right Performed by the same Process
void PushRight(int val){
    node* NewNode = CreateNode(val);
    while(1){
        anchor *MyDeck= Deck;
        node* Lend= MyDeck->LMost;
        node* Rend= MyDeck->RMost;
        status S= MyDeck->sta;
        if (Rend==NULL){
            if(CAS(&Deck,MyDeck,SetDeck(NewNode,NewNode,S))==MyDeck){
                return;
            }
        }
        else if(S==Stable){ //The Deck is stable and has one or more elements
            NewNode->left=Rend;
            anchor *NewDeck= SetDeck(Lend,NewNode,Rpush);
            if(CAS(&Deck,MyDeck,NewDeck)==MyDeck){
                StablizeRight(NewDeck);//Make the last right most to point to new right most then make the Deck Stable
                return;
            }
        }
        else{
            Stablize(MyDeck);// Help the Deck be stable
        }
    }
}

//Push Left May not be used
void PushLeft(int val){
    node* NewNode = CreateNode(val);
    while(1){
        anchor *MyDeck= Deck;
        node* Lend= MyDeck->LMost;
        node* Rend= MyDeck->RMost;
        status S= MyDeck->sta;
        if (Lend==NULL){//Deque is empty
            if(CAS(&Deck,MyDeck,SetDeck(NewNode,NewNode,S))==MyDeck){
                return;
            }
        }
        else if(S==Stable){ //The Deck is stable and has one or more elements
            NewNode->right=Lend;
            anchor *NewDeck= SetDeck(NewNode,Rend,Lpush);
            if(CAS(&Deck,MyDeck,NewDeck)==MyDeck){
                StablizeLeft(NewDeck);//Make the last left most to point to new left most then make the Deck Stable
                return;
            }
        }
        else{
            Stablize(MyDeck);// Help the Deck be stable
        }
    }
}

//Only local pop can occur from the right
int PopRight(){
    anchor *MyFinalDeck;
    while(1){
        anchor *MyDeck= Deck;
        node* Lend= MyDeck->LMost;
        node* Rend= MyDeck->RMost;
        status S= MyDeck->sta;
        if(Rend==NULL) return -1; //Empty deque
        if(Rend==Lend){
            if(CAS(&Deck, MyDeck, SetDeck(NULL,NULL,Stable))==MyDeck){
                MyFinalDeck=MyDeck;
                break;
            }
        }
        else if(S==Stable){
            if (Deck != MyDeck) continue;
            node *NewRend = Rend->left;
            if(CAS(&Deck, MyDeck,SetDeck(Lend,NewRend,Stable))==MyDeck){
                MyFinalDeck=MyDeck;
                break;
            }
        }
        else{
            Stablize(MyDeck);
        }
    }
    int data = MyFinalDeck->RMost->val;
    //free(MyFinalDeck);
    return data;
}

//Only remote pop can occur from the left
int PopLeft(){
    anchor *MyFinalDeck;
    while(1){
        anchor *MyDeck= Deck;
        node* Lend= MyDeck->LMost;
        node* Rend= MyDeck->RMost;
        status S= MyDeck->sta;
        if(Lend==NULL) return -1; //Empty deque
        if(Rend==Lend){
            if(CAS(&Deck, MyDeck, SetDeck(NULL,NULL,Stable))==MyDeck){
                MyFinalDeck=MyDeck;
                break;
            }
        }
        else if(S==Stable){
            if (Deck != MyDeck) continue;
            node *NewLend = Lend->right;
            if(CAS(&Deck, MyDeck,SetDeck(NewLend,Rend,Stable))==MyDeck){
                MyFinalDeck=MyDeck;
                break;
            }
        }
        else{
            Stablize(MyDeck);
        }
    }
    int data = MyFinalDeck->LMost->val;
    //free(MyFinalDeck);
    return data;
}

void wait_flags(volatile uint32_t *flag, uint32_t Id){
    __sync_fetch_and_add((uint32_t *)flag,1);
    while(*flag!=Id){
        cpu_relax();
    }
}
//static uint32_t EnqueueVal = 0;
void *Thread_Func(void* Idx){
    int n = (int)Iter/Nthrds;
    wait_flags(&wflag,Nthrds);
    if(Idx==0){
        gettimeofday(&StartTime, NULL);
    }
    for(long i;i<n;i++){
        int random=rand();
        if(random%4==1){
            PushRight(i+((long)Idx*n));
        }
        else if(random%4==2){
            PushLeft(i+((long)Idx*n));
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
int main(int argc, const char *argv[]){
    if (argc != 2) {
        printf("Usage: %s <num of threads>\n", argv[0]);
        exit(1);
    }
    Nthrds = atoi(argv[1]); // Getting the number of threads
    Deck= SetDeck(NULL,NULL,Stable);//Initialzing the Deque
    pthread_t *thr;
    thr = calloc(Nthrds,sizeof(*thr));
    for(long i=0; i<Nthrds; i++){
        if(pthread_create(&thr[i],NULL,Thread_Func,(void *)i)!=0){
            perror("Thread Create Failed");
        }
    }
    for(long i=0; i<Nthrds; i++){
        pthread_join(thr[i],NULL);
    }
    node *CurNode=Deck->LMost;
    printf("\n");
    int Len=get_len();
    printf("\nThe length of Deck is %d",Len);
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