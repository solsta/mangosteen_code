#include "../../mangosteen_instrumentation.h"
#include "../../flat_combining/dr_annotations.h"

#include <stdio.h>
#include <string.h>
#include <atomic>
#include <cstdio>  
#include <thread>
#include <chrono>
#include "/home/se00598/home/se00598/vanila_flit/flit/common/rand_r_32.h"
#include "ssmem_wrapper.hpp"

//#include "/home/se00598/home/se00598/vanila_flit/flit/common/ssmem_wrapper.hpp"
#include <vector>
#include <mutex>

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>


#include <random>
#include <chrono>
#include <thread>

#include <assert.h>
#include <atomic>
#include <bits/stdc++.h> 

//#define RUN_WITH_MANGOSTEEN
#define SM_OP_INSERT 0
#define SM_OP_DELETE 1
#define SM_OP_READ 2

#define ALIGNMENT 128

// The class code original code from https://github.com/cmuparlay/flit
template <class T> 
class alignas(ALIGNMENT) ListOriginal{
private:
    class alignas(ALIGNMENT) Node{
    public:
        int key;
        T value;
        std::atomic<Node*> next;

        Node(int k, T val, Node* n) : key(k), value(val), next(n) {
            // std::cout << "Node alignment: " << (((uintptr_t) this) & -((uintptr_t) this)) << ", expected alignment " << ALIGNMENT << std::endl;
            assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
        }
        Node* getNext() {
            return next.load(std::memory_order_acquire);
        }
        bool CAS_next(Node* exp, Node* n) {
            return next.compare_exchange_strong(exp, n);
        }
    };

//===========================================

    class Window {
    public:
        Node* pred;
        Node* curr;
        Window(Node* myPred, Node* myCurr) {
            pred = myPred;
            curr = myCurr;
        }
    };

    Node* head;

//===========================================

    Node* getAdd(Node* n) {
    long node = (long)n;
        return (Node*)(node & ~(0x1L)); // clear bit to get the real address
    }

    bool getMark(Node* n) {
        long node = (long)n;
        return (bool)(node & 0x1L);
    }

    Node* mark(Node* n) {
        long node = (long)n;
        node |= 0x1L;
        return (Node*)node;
    }

public:

    ListOriginal(int) : ListOriginal() {}

    ListOriginal() {
        head = static_cast<Node*>(ssmem.alloc(sizeof(Node), false));
        new (head) Node(INT_MIN, INT_MIN, NULL);
        assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
    }

    ~ListOriginal() {
        while(head != NULL) {
            Node* next = getAdd(head->getNext());
            //ssmem.free(head); // figure out why this doesn't work with hashtable
            head = next;
        }
    }
    
//===========================================

        Window seek(Node* head, int key) {
            Node* left = head;
            Node* leftNext = head->getNext();
            Node* right = NULL;
            
            Node* curr = NULL;
            Node* currAdd = NULL;
            Node* succ = NULL;
            bool marked = false;
            while (true) {
                curr = head;
                currAdd = curr;
                succ = currAdd->getNext();
                marked = getMark(succ);
                /* 1: Find left and right */
                while (marked || currAdd->key < key) {
                    if (!marked) {
                        left = currAdd;
                        leftNext = succ;
                    }
                    curr = succ;
                    currAdd = getAdd(curr);        // load_acq
                    if (currAdd == NULL) {
                        break;
                    }
                    succ = currAdd->getNext();
                    marked = getMark(succ);
                }
                right = currAdd;
                /* 2: Check nodes are adjacent */
                if (leftNext == right) {
                   if ((right != NULL) && getMark(right->getNext())) {
                    continue;
                } else {
                    return Window(left, right);
                }
            }
                /* 3: Remove one or more marked nodes */
            if (left->CAS_next(leftNext, right)) {
                Node* removedNode = getAdd(leftNext);
                while(removedNode != right) {
                    ssmem.free(removedNode, false);
                    removedNode = getAdd(removedNode->getNext());
                }
                if ((right != NULL) && getMark(right->getNext())) {
                    continue;
                } else {
                    return Window(left, right);
                }
            }
        }
    }

//=========================================

    bool add(int k, T item) {
        //bool add(T item, int k, int threadID) {
        while (true) {
            Window window = seek(head, k);
            Node* pred = window.pred;
            Node* curr = window.curr;
            if (curr && curr->key == k) {
                return false;
            }
            Node* node = static_cast<Node*>(ssmem.alloc(sizeof(Node), false));
            new (node) Node(k, item, curr);
            bool res = pred->CAS_next(curr, node);
            if (res) {
                return true;
            } else {
                ssmem.free(node, false);
                continue;
            }
        }
    }

//========================================

    bool remove(int key) {
        bool snip = false;
        while (true) {
            Window window = seek(head, key);
            Node* pred = window.pred;
            Node* curr = window.curr;
            if (!curr || curr->key != key) {
                return false;
            } else {
                Node* succ = curr->getNext();
                Node* succAndMark = mark(succ);
                if (succ == succAndMark) {
                    continue;
                }
                snip = curr->CAS_next(succ, succAndMark);
                if (!snip)
                    continue;
                if(pred->CAS_next(curr, succ))           //succ is not marked
                    ssmem.free(curr, false);
                return true;
            }
        }
    }

//========================================

        bool contains(int k) {
            int key = k;
            Node* curr = head;
            bool marked = getMark(curr->getNext());
            while (curr->key < key) {
                curr = getAdd(curr->getNext());
                if (!curr) {
                    return false;
                }
                marked = getMark(curr->getNext());
            }
            if(curr->key == key && !marked){
                return true;
            } else {
                return false;
            }
        }

        static std::string get_name() {
            return "List Original";
        }

        /* Functions for debugging and validation.
           Must be run in a quiescent state.
         */

        long long size() {
            long long s = 0;
            Node* n = getAdd(head->getNext());
            while(n != nullptr) {
                bool marked = getMark(n->getNext());
                if(!marked) {
                    //std::cout << "El:" << n->value << std::endl;
                    s++;
                }
                n = getAdd(n->getNext());
            }
            return s;
        }

        long long keySum() {
            long long s = 0;
            Node* n = getAdd(head->getNext());
            while(n != nullptr) {
                bool marked = getMark(n->getNext());
                if(!marked) s+=n->key;
                n = getAdd(n->getNext());
            }
            return s;
        }
};
ListOriginal<int> list;




bool isReadOnly(serialized_app_command *serializedAppCommand){
    if(serializedAppCommand->op_type == SM_OP_READ){
        return true;
    }
    return false;
}



void processRequest(serialized_app_command *serializedAppCommand){
    

    if(serializedAppCommand->op_type == SM_OP_INSERT){
        list.add(serializedAppCommand->key, serializedAppCommand->key);
    } else if(serializedAppCommand->op_type == SM_OP_DELETE){
        list.remove(serializedAppCommand->key);
    } else{
        printf("Unknown command\n");
        exit(EXIT_FAILURE);
    }
}

void benchmark(int num_threads, int ops_per_thread) {
    
    ssmem.alloc(ALIGNMENT);
    my_rand::init(42);
    while (list.size() < 128){
        int value = my_rand::get_rand()%256;
        list.add(value, value);
    }

    std::cout << "List size before : " << list.size() << std::endl;


#ifdef RUN_WITH_MANGOSTEEN
mangosteen_args mangosteenArgs;
mangosteenArgs.isReadOnly = &isReadOnly;
mangosteenArgs.processRequest = &processRequest;
mangosteenArgs.mode = MULTI_THREAD;

initialise_mangosteen(&mangosteenArgs);
printf("Mangosteen has initialized\n");
#endif

    auto worker = [&](int thread_id) {
        my_rand::init(thread_id);
        ssmem.alloc(ALIGNMENT);
#ifdef RUN_WITH_MANGOSTEEN
        mangosteen_args *mangosteenArgs;
        initialise_mangosteen(mangosteenArgs);
        serialized_app_command serializedAppCommand;
#endif   
        for (int i = 0; i < ops_per_thread; ++i) {

            
            int op = my_rand::get_rand()%100;
            int value = my_rand::get_rand()%256;

            if (op <= 50){
#ifdef RUN_WITH_MANGOSTEEN
                serializedAppCommand.key = value;
                serializedAppCommand.op_type = SM_OP_INSERT;
#else
                list.add(value, value);
#endif
                
            } else {
#ifdef RUN_WITH_MANGOSTEEN
                serializedAppCommand.key = value;
                serializedAppCommand.op_type = SM_OP_DELETE;
#else
                list.remove(value);
#endif
                
            }
#ifdef RUN_WITH_MANGOSTEEN
            clientCmd(&serializedAppCommand);
#endif
            
        }
    };

    std::vector<std::thread> threads;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_threads; ++i){
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads)
        t.join();

    //std::cout << "List size after : " << list.size() << std::endl;
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::chrono::duration<double> duration_sec = end_time - start_time;
    int totalOps = num_threads * ops_per_thread;
    std::cout << "Threads: " << num_threads
              << ", Ops/thread: " << ops_per_thread
              << ", Total ops: " << num_threads * ops_per_thread
              << "\nTime: " << duration.count() << " ms\n";


    double elapsedSec = std::chrono::duration<double>(end_time - start_time).count();
    printf("Throughput: %.2f ops/sec\n", totalOps / elapsedSec);

    std::cout << "List size after : " << list.size() << std::endl;
}


int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <num_threads>\n";
        return 1;
    }

    int num_threads = std::atoi(argv[1]);
    if (num_threads <= 0) {
        std::cerr << "Number of threads must be positive.\n";
        return 1;
    }

    int ops_per_thread = 1000000;

    benchmark(num_threads, ops_per_thread);
    return 0;
}