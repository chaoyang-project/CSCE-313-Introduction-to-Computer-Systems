#include "common.h"
#include <sys/wait.h>
#include "FIFOreqchannel.h"
#include "BoundedBuffer.h"
#include "HistogramCollection.h"
#include <thread>
#include "Histogram.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <time.h>
#include <unordered_map>

using namespace std;
struct Response{
    int person;
    double ecgval;

    Response (int _p, double _e): person (_p), ecgval (_e){;}

};
void timediff (struct timeval& start, struct timeval& end){
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;
}
FIFORequestChannel* create_new_channel (FIFORequestChannel* mainchan){
    char name [1024];
    MESSAGE_TYPE m = NEWCHANNEL_MSG;
    mainchan->cwrite (&m, sizeof (m));
    mainchan->cread (name, 1024);
    FIFORequestChannel* newchan = new FIFORequestChannel (name, FIFORequestChannel::CLIENT_SIDE);
    return newchan;
}

void patient_thread_function (int n, int p, BoundedBuffer* reqbuffer){
    double t = 0;
    datamsg d (p, t, 1);
    for (int i=0; i<n; i++){
        reqbuffer->push ((char*) &d, sizeof (d));
        d.seconds += 0.004;
    }
    
}

void file_thread_function(string filename, BoundedBuffer* request_buffer, FIFORequestChannel* chan, int mb) {
  
    char buf[1024];
    filemsg f(0, 0);
    memcpy(buf, &f, sizeof(f));
    strcpy(buf + sizeof(f), filename.c_str());
    int sz = sizeof(f) + filename.size() + 1;
    chan->cwrite(buf, sz);
    __int64_t filelength;
    chan->cread(&filelength, sizeof(filelength));
    
    // create output file
    string recvfname = "received/" + filename;
    FILE* fp = fopen(recvfname.c_str(), "w");
    fseek(fp, filelength, SEEK_SET);
    fclose(fp);

    // generate all the filemsgs
    filemsg* fm = (filemsg*)buf;
    __int64_t remlen = filelength;

    while(remlen > 0) {
        fm->length = min(remlen, (__int64_t)mb);
        request_buffer->push(buf, sz);
        fm->offset += fm->length;
        remlen -= fm->length;
    }
}

void event_polling_thread(int w, int mb, FIFORequestChannel** wchans, BoundedBuffer* request_buffer, BoundedBuffer* response_buffer){
    char buf[1024];
    double resp = 0;
    char recvbuf[mb];

    struct epoll_event ev;
    struct epoll_event events[w];

    int epollfd = epoll_create1(0);
    if(epollfd == -1) {
        EXITONERROR("epoll_create1");
    }

    unordered_map<int, int> fd_to_index;
    vector<vector<char>> state(w);

    bool quit_recv = false;
    int nsent = 0;
    int nrecv = 0;

    // priming 
    for(int i = 0; i < w; i++) {
        int sz = request_buffer->pop(buf, 1024);
        wchans[i]->cwrite(buf, sz);
        
        // state management 
        state[i] = vector<char>(buf, buf+sz);
        nsent++;
        int rfd = wchans[i]->getrfd();
        fd_to_index[rfd] = i;
        fcntl(rfd, F_SETFL, O_NONBLOCK);

        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = rfd;

        if(epoll_ctl(epollfd, EPOLL_CTL_ADD, rfd, &ev) == -1) {
            EXITONERROR("epoll_ctl: listen_sock");
        }
    }

    while(true) {
        if(quit_recv && nrecv == nsent) {
            break;
        }
        int nfds = epoll_wait(epollfd, events, w, -1);
        if(nfds == -1) {
            EXITONERROR("epoll_wait");
        }
        for(int i = 0; i < nfds; i++) {
            int rfd = events[i].data.fd;
            int index = fd_to_index[rfd];
            int resp = wchans[index]->cread(recvbuf, mb);
            nrecv++;
            vector<char> req = state[index];
            char* request = req.data();

            MESSAGE_TYPE* m = (MESSAGE_TYPE*) request;
            if(*m == DATA_MSG) {
                Response r {((datamsg*)request)->person, *(double*)recvbuf};
                response_buffer->push((char*)&r, sizeof(r));
            } else if(*m == FILE_MSG) {
                filemsg* fm = (filemsg*)request;
                string fname = (char*)(fm+1);
                string recvfname = "received/" + fname;
                FILE* fp = fopen(recvfname.c_str(), "r+");
                fseek(fp, fm->offset, SEEK_SET);
                fwrite(recvbuf, 1, fm->length, fp);
                fclose(fp);
            }
            // channel reuse 
            if(!quit_recv) {
                int req_sz = request_buffer->pop(buf, sizeof(buf));
                wchans[index]->cwrite(buf, req_sz);
                nsent++;
                state[index] = vector<char>(buf, buf + req_sz);         
                if (*(MESSAGE_TYPE *) buf == QUIT_MSG) {
                    quit_recv = true;
                }
            }
        }
    }
}

void histogram_thread_function (BoundedBuffer* responseBuffer, HistogramCollection* hc){
    char buf [1024];
    Response* r = (Response *) buf;
    while (true){
        responseBuffer->pop (buf, 1024);
        if (r->person < 1){ // it means quit
            break;
        }
        hc->update (r->person, r->ecgval);
    }
}



int main(int argc, char *argv[]){
    
    int c;
    int buffercap = MAX_MESSAGE;
    int p = 10, ecg = 1;
    double t = -1.0;
    bool isnewchan = false;
    bool isfiletransfer = false;
    string filename;
    int b = 1024;
    int w = 100;
    int n = 10000;
    int m = MAX_MESSAGE;
    int h = 3;


    while ((c = getopt (argc, argv, "p:t:e:m:f:b:cw:n:h:")) != -1){
        switch (c){
            case 'p':
                p = atoi (optarg);
                break;
            case 't':
                t = atof (optarg);
                break;
            case 'e':
                ecg = atoi (optarg);
                break;
            case 'm':
                buffercap = atoi (optarg);
                m = buffercap;
                break;
            case 'c':
                isnewchan = true;
                break;
            case 'f':
                isfiletransfer = true;
                filename = optarg;
                break;
            case 'b':
                b = atoi (optarg);
                break;
            case 'w':
                w = atoi (optarg);
                break;
            case 'n':
                n = atoi (optarg);
                break;
            case 'h':
                h = atoi (optarg);
                break;
        }
    }
    
    // fork part
    if (fork()==0){ // child 
	
		char* args [] = {"./server", "-m", (char *) to_string(buffercap).c_str(), NULL};
        if (execvp (args [0], args) < 0){
            perror ("exec filed");
            exit (0);
        }
    }

    FIFORequestChannel* chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    BoundedBuffer requestBuffer (b);
	BoundedBuffer responseBuffer (b);
    HistogramCollection hc;

    // making histograms and adding to the histogram collection hc
    for (int i=0; i<p; i++){
        Histogram* h = new Histogram (10, -2.0, 2.0);
        hc.add (h);
    }

    // make w worker channels (make sure to do it sequentially in the main)
    // FIFORequestChannel* wchans [w];
    FIFORequestChannel** wchans = new FIFORequestChannel*[w];
    for (int i=0; i<w; i++){
        wchans [i] = create_new_channel (chan);     
    }
	
	
    struct timeval start, end;
    gettimeofday (&start, 0);
    /* Start all threads here */
    if (!isfiletransfer) {
        thread patient [p];
        for (int i=0; i<p; i++){
            patient [i] = thread (patient_thread_function, n, i+1, &requestBuffer);
        }
        thread evp(event_polling_thread, w, m, wchans, &requestBuffer, &responseBuffer);
        thread hists [h];
        for (int i=0; i<h; i++){
            hists [i] = thread (histogram_thread_function, &responseBuffer, &hc);
        }
        /* Join all threads here */
        for (int i=0; i<p; i++){
            patient [i].join ();
        }
        cout << "Patient threads finished" << endl;

        MESSAGE_TYPE Q = QUIT_MSG;
        // requestBuffer.push ((char*) &Q, sizeof (Q));
        if (w > (p*n)) { // prevent deadlocking
            // sleep(1);
            int num_good = p*n;
            for (int i = num_good; i <= w; i++) {
                requestBuffer.push((char*)&Q, sizeof (Q));
            }
        } else {
            requestBuffer.push((char*)&Q, sizeof (Q));
        }

        evp.join();
        cout << "Event polling thread finished" << endl;
        for (int i=0; i<h; i++){
            datamsg d(-1, 0, -1);
            responseBuffer.push ((char*)&d, sizeof (d));
        }
        for (int i=0; i<h; i++){
            hists [i].join ();
        }
        cout << "Histogram threads are done. All client threads are now done" << endl;

        gettimeofday (&end, 0);
        // print time difference
        timediff (start, end);
        // print the results
        hc.print ();
    }
    if (isfiletransfer) {
        thread filethread = thread(file_thread_function, filename, &requestBuffer, chan, m);
        thread evp(event_polling_thread, w, m, wchans, &requestBuffer, &responseBuffer);
        filethread.join();

        MESSAGE_TYPE Q = QUIT_MSG;
        if (w > (p*n)) { // prevent deadlocking
            cout << "special case" << endl;
            int num_good = p*n;
            for (int i = num_good; i <= w; i++) {
                requestBuffer.push((char*)&Q, sizeof (Q));
            }
        } else {
            requestBuffer.push((char*)&Q, sizeof (Q));
        }

        evp.join();
        cout << "Event polling thread finished" << endl;

        gettimeofday (&end, 0);
        // print time difference
        timediff (start, end);
    }

    for (int i=0; i<w; i++) {
        delete wchans[i];
    }
    delete[] wchans;

    // cleaning the main channel
    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    wait (0);
    cout << "All Done!!!" << endl;

    delete chan;
   
}
