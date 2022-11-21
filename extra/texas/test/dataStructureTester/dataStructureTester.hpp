#include <map>
#include <unordered_map>
#include "splay-tree.h"
#include "linked_list.hpp"
#include "btree.hpp"

#include <cstdint>
#include <iostream>
#include <stdio.h>


enum{
    SPLAY_TREE = 0,
    RB_TREE,
    HASH_MAP,
    LINKED_LIST,
    B_TREE,
};

map<std::string, uint64_t> implementations = {
    {"SPLAY_TREE", SPLAY_TREE},
    {"RB_TREE", RB_TREE},
    {"HASH_MAP", HASH_MAP},
    {"LINKED_LIST", LINKED_LIST},
    {"B_TREE", B_TREE},
};



class dataStructure {
    public:
        void insert(uint64_t);
        void remove(uint64_t);
        void* find(uint64_t);

        uint64_t impl = 0;
        void* tree = nullptr;

        dataStructure(uint64_t);

};

void* dataStructure::find(uint64_t key){
    void* returner = nullptr;
    switch(impl){
        case SPLAY_TREE:
            {
                struct splay_tree_key_s lookup_key;
                lookup_key.key = key;
                returner = (void*) splay_tree_lookup((splay_tree) tree, &lookup_key);
                break;
            }
        case RB_TREE:
            {
                auto entry = ((std::map<uint64_t, void*>*)tree)->find(key);
                if(entry != ((std::map<uint64_t, void*>*)tree)->end() ){
                    returner = entry->second;
                }
                break;
            }
        case HASH_MAP:
            {
                auto entry = ((std::unordered_map<uint64_t, void*>*)tree)->find(key);
                if(entry != ((std::unordered_map<uint64_t, void*>*)tree)->end()){
                    returner = entry->second;
                }
                break;
            }
        case LINKED_LIST:
            {
                returner = (void*)((linked_list*)tree)->findNode(key);
                break;
            }
        case B_TREE:
            {
                break;
            }
    }

    return returner;
}

void dataStructure::remove(uint64_t key){
    switch(impl){
        case SPLAY_TREE:
            {
                struct splay_tree_key_s lookup_key;
                lookup_key.key = key;
                splay_tree_remove((splay_tree)tree, &lookup_key);
                break;
            }
        case RB_TREE:
            {
                ((std::map<uint64_t, void*>*)tree)->erase(key);
                break;
            }
        case HASH_MAP:
            { 
                ((std::unordered_map<uint64_t, void*>*)tree)->erase(key);
                break;
            }
        case LINKED_LIST:
            {
                ((linked_list*)tree)->removeNode(key);
                break;
            }
        case B_TREE:
            {
                break;
            }
    }
}

void dataStructure::insert(uint64_t key){
    splay_tree_node n = (splay_tree_node) malloc(sizeof(*n));
    n->key.key=key;
    
    switch(impl){
        case SPLAY_TREE:
            {
                splay_tree_insert((splay_tree)tree, n);
                break;
            }
        case RB_TREE:
            { 
                ((std::map<uint64_t, void*>*)tree)->insert_or_assign(key, (void*)n);
                break;
            }
        case HASH_MAP:
            { 
                ((std::unordered_map<uint64_t, void*>*)tree)->insert_or_assign(key, (void*)n);
                break;
            }
        case LINKED_LIST:
            {
                ((linked_list*)tree)->insertNode(((linked_list*)tree)->allocateNode(key));
                break;
            }
        case B_TREE:
            { 
                break;
            }
    }
}

dataStructure::dataStructure(uint64_t imple){
    impl = imple;
    switch(impl){
        case SPLAY_TREE:
            {
                tree = (void*) malloc(sizeof(splay_tree));
                break;
            }
        case RB_TREE:
            { 
                tree = (void*) new std::map<uint64_t, void*>();
                break;
            }
        case HASH_MAP:
            { 
                tree = (void*) new std::unordered_map<uint64_t, void*>();
                break;
            }
        case LINKED_LIST:
            { 
                tree = (void*) new linked_list();
                break;
            }
        case B_TREE:
            { 
                tree = (void*) new btree();  
                break;
            }
    }
}


