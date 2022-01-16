#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <vector>
#include "Semaphore.h"

#define THINK 2
#define HUNGRY 1
#define EAT 0

Semaphore* me = new Semaphore(1);
vector<Semaphore*> semaphore;
int pflag[5];
int p[5] = {0, 1, 2, 3, 4};

void test(int num)
{
    if (pflag[num] == HUNGRY && pflag[(num + 4) % 5] != EAT && pflag[(num + 1) % 5] != EAT) {
        pflag[num] = EAT;
        sleep(2);
        printf("Philosopher %d takes chopsticks %d and %d up\n", num + 1, ((num + 4) % 5) + 1, num + 1);
        semaphore[num]->V();
    }
}

void take_chopsticks(int num)
{
    me->P();
    pflag[num] = HUNGRY;
    test(num);
    me->V();
    semaphore[num]->P();
    sleep(1);
}

void drop_chopsticks(int num)
{
    me->P();
    pflag[num] = THINK;
    printf("Philosopher %d puts chopsticks %d and %d down\n", num + 1, ((num + 4) % 5) + 1, num + 1);
    test((num + 4) % 5);
    test((num + 1) % 5);
    me->V();
}

int* procedure(int* num)
{
    while (1) {
        int* i = (int *)num;
        sleep(1);
        take_chopsticks(*i);
        sleep(0);
        drop_chopsticks(*i);
    }
}

int main()
{
    pthread_t threads[5];
    for(int i = 0; i < 5; i++) {
        semaphore.push_back(new Semaphore(0));
    }
    for (int i = 0; i < 5; i++) {
        pthread_create(&threads[i], NULL, (void* (*)(void*))procedure, &p[i]);
    }
    for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }
    // delete        
    delete me;
    for(int i = 0; i < 5; i++) {
        delete semaphore[i];
    }
}
