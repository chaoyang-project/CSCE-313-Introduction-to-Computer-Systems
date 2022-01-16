#include <iostream>
#include <thread>
#include <unistd.h>
#include <vector>
#include "Semaphore.h"
using namespace std;

Semaphore Adone (0);
Semaphore Bdone (0);
Semaphore Cdone (1);
Semaphore mtx (1);
int Bcount = 0;

void A (){
    while (true){
    Cdone.P();
    cout << "A thread is running " << endl;
    Adone.V();
    Adone.V();
    }
}

void B (){
    while (true){
        Adone.P();
        mtx.P();
        cout << "B thread is running " << endl;
        Bcount ++;
        if (Bcount == 2){
            Bcount = 0;
            Bdone.V();
        }
        mtx.V();
    }
}

void C (){
    while (true){
        Bdone.P();
        cout << "C thread is running " << endl << endl;
        Cdone.V();
    }
}

int main (){
    vector<thread> producerAs;
    vector<thread> consumerBs;
    vector<thread> consumerCs;
    for (int i=0; i<25; i++){
        producerAs.push_back(thread(A));
        consumerBs.push_back(thread(B));
        consumerCs.push_back(thread(C));
    }
    for (int i=0; i<25; i++){
        producerAs [i].join ();
        consumerBs [i].join ();
        consumerCs [i].join ();
    }
}