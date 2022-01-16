#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
using namespace std;

class Semaphore{
private:
    int value;
    mutex m;
    condition_variable cv;
public:
    Semaphore (int _v):value(_v){}
    void P(){
		unique_lock<mutex> l (m);
		cv.wait (l, [this]{return value > 0;});
        value --;
    }
    void V(){
        unique_lock<mutex> l(m);
        value ++;
        cv.notify_all(); /* this is notifying all to make output of dependent programs interesting
                            although a better method would be to call notify_one() */
                            
    }
};