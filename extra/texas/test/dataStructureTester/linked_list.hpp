#include "stdint.h"

struct __attribute__ ((packed)) ll_node
{
    volatile uint64_t key;
    //This is 32KB - 16 bytes
    volatile uint64_t filler[4094];
    ll_node* next = nullptr;
};

class linked_list{
    public:
        ll_node* head = nullptr;

        void insertNode(ll_node*);
        void removeNode(uint64_t);
        ll_node* findNode(uint64_t);
        ll_node* allocateNode(uint64_t);

};


void linked_list::insertNode(ll_node* newNode){
    if(head == nullptr){
        head = newNode;
        return;
    }
    //Find the end of the list
    ll_node* iter = head;
    while(iter->next != nullptr){
        if(iter->key == newNode->key){
            printf("Duplicate key!\n");
            return;
        }
        iter = iter->next;
    }
    //Found it
    iter->next = newNode;
    return;

}

void linked_list::removeNode(uint64_t key){
    if (head == nullptr){
        return;
    } 

    ll_node* iter = head;
    ll_node* prev = head;
    //Find the ll_node
    while(iter != nullptr){
       
        if(iter->key == key){
            break;
        } 
        prev = iter;
        iter = iter->next;
    }
    //Found it
    prev->next = iter->next;
    free(iter);
    return;

}

ll_node* linked_list::findNode(uint64_t key){
    if (head == nullptr){
        return nullptr;
    }
    ll_node* iter = head;
    while(iter != nullptr){
        if(iter->key == key){
            break;
        }
        iter = iter->next;
    }
    //Found it
    return iter;
}

ll_node* linked_list::allocateNode(uint64_t key){
    ll_node* newNode = (ll_node*)malloc(sizeof(ll_node));
    newNode->key = key;
    return newNode;        
}
