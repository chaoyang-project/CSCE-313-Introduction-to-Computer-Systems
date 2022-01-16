/*
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date  : 2/8/20
 */
#include <sys/wait.h>
#include "common.h"
#include "FIFOreqchannel.h"


using namespace std;


int main(int argc, char *argv[]){
    
	
	int opt;
	int p = 1;
	double t = 0.0;
	int e = 1;
	int n = 1;
	string filename = "";
	int buffercapacity = MAX_MESSAGE;
	char* buffercapacityChar = "";
	int newChannel = 0;
	

	while ((opt = getopt(argc, argv, "p:t:e:f:n:m:c:")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'n':
				n = atoi (optarg);
				break;
			case 'm':
				buffercapacity = atoi (optarg);	
				buffercapacityChar = optarg;
				break;
			case 'c':
				newChannel = atoi (optarg);
				break;	
		}
	}
	
	int pid = fork();
	if (pid == 0) {  // child process
		char* args[4];
		args[0] = "./server";
		if (buffercapacity == MAX_MESSAGE) {
			args[1] = NULL;
		} else {
			args[1] = "-m";
			args[2] = buffercapacityChar ;
			args[3] = NULL;
		}
		execvp("./server", args);		
	} else {  // parent process
		FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);
		if ( n == 1 && filename.size() == 0 && newChannel == 0) {
			// request 1 person 1 ecg value
			datamsg x (p, t, e);
			chan.cwrite (&x, sizeof (datamsg)); // question
			double reply;
			int nbytes = chan.cread (&reply, sizeof(double)); //answer
			cout << "For person " << p <<", at time " << t << ", the value of ecg "<< e <<" is " << reply << endl;
			// cout << nbytes << endl;
		} else if (n != 1 && filename.size() == 0 && newChannel == 0) {
			// request n data points
			cout << "Number of data points: " << n << endl;
			ofstream outfile("./x1.csv");
			timeval start, end;
			gettimeofday(&start, NULL);
			for (int i = 0; i < n; i++) {
				t = i*0.004;
				datamsg x1 (p, t, 1);
				datamsg x2 (p, t, 2);
				chan.cwrite (&x1, sizeof (datamsg));
				double reply1;
				int nbytes1 = chan.cread (&reply1, sizeof(double)); //answer
				// cout << "For person " << p <<", at time " << t << ", the value of ecg "<< "1" <<" is " << reply1 << endl;
				chan.cwrite (&x2, sizeof (datamsg));
				double reply2;
				int nbytes2 = chan.cread (&reply2, sizeof(double)); //answer
				// cout << "For person " << p <<", at time " << t << ", the value of ecg "<< "2" <<" is " << reply2 << endl;
				outfile << i * 0.004 << "," << reply1 << "," << reply2 << endl;
			}
			gettimeofday(&end, NULL);
			double time = end.tv_sec - start.tv_sec;
			cout << "Time consumption is: " << time << " seconds" << endl;
			outfile.close();
		} else if (filename.size() != 0 && newChannel == 0) {
			// request file
			
			// request file length; 
			filemsg fm (0,0); 
			int len = sizeof (filemsg) + filename.size()+1;
			char buf2 [len];
			memcpy (buf2, &fm, sizeof (filemsg));
			strcpy (buf2 + sizeof (filemsg), filename.c_str());
			chan.cwrite (buf2, len);
				
			int64_t fileLength;
			chan.cread(&fileLength, sizeof(int64_t));
			cout << "File length: " << fileLength << " bytes" << endl;
			
			// request file content;
			int64_t offset = 0;
			int length;
			ofstream outfile("./received/" + filename, ios::out | ios::binary);
			char buf3 [buffercapacity];
			timeval start, end;
			gettimeofday(&start, NULL);
			while (offset < fileLength) {
				if (fileLength - offset >= buffercapacity) {
					length = buffercapacity;
				} else {
					length = fileLength - offset;
				}
				filemsg filePiece (offset, length);
				memcpy (buf2, &filePiece, sizeof (filemsg));
				strcpy (buf2 + sizeof (filemsg), filename.c_str());
				chan.cwrite (buf2, len);
				chan.cread (buf3, length);
				outfile.write(buf3, length);
				offset += length;
			}
			gettimeofday(&end, NULL);
			double time = (end.tv_sec - start.tv_sec)*1000000 + end.tv_usec - start.tv_usec;
			cout << "Time consumption is: " << time << " microseconds" << endl;
			outfile.close();
		} else if (newChannel == 1) {
			// request a new channel
			MESSAGE_TYPE newChannel_MSG = NEWCHANNEL_MSG;
			chan.cwrite(&newChannel_MSG, sizeof(MESSAGE_TYPE));
			char *replyNewChannel = new char[30];
			chan.cread(replyNewChannel, 30);
			FIFORequestChannel new_chan (replyNewChannel, FIFORequestChannel::CLIENT_SIDE);
			cout << "new channel created" << endl;
			datamsg xnew (2, 0, 1);
			new_chan.cwrite (&xnew, sizeof (datamsg)); // question
			double replyNew;
			new_chan.cread (&replyNew, sizeof(double)); //answer
			cout << "For person 2, at time 0, the value of ecg 1 is " << replyNew << endl;
			// closing the channel    
			MESSAGE_TYPE m2 = QUIT_MSG;
			new_chan.cwrite (&m2, sizeof (MESSAGE_TYPE));
			cout << "new channel closed" << endl;

			delete replyNewChannel;
		}


		// closing the channel    
		MESSAGE_TYPE m = QUIT_MSG;
		chan.cwrite (&m, sizeof (MESSAGE_TYPE));
		wait(0);
	}
}
