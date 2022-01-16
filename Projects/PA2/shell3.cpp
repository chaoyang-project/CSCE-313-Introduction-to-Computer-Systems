#include <iostream>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
using namespace std;

string trim (string input) {
    int i = 0;
    while (i < input.size() && input[i] == ' ') {
        i++;
    }
    if (i < input.size()) {
        input = input.substr(i);
    } else {
        return "";
    }

    i = input.size() - 1;
    while (i > 0 && input[i] == ' ') {
        i--;
    }
    if (i >= 0) {
        input = input.substr(0, i+1);
    } else {
        return "";
    }

    return input;
}

vector<string> split (string line, string separator = " ") {
    vector<string> result;
    while(line.size()) {
        size_t found = line.find(separator);
        if (found == string::npos) {
            string lastpart = trim(line);
            if (lastpart.size()>0){
                result.push_back(lastpart);
            }
            break;
        }
        string segment = trim(line.substr(0,found));
        line = line.substr(found+1);
        if (segment.size()!=0) {
            result.push_back (segment);
        }
    }
    return result;
}

char** vec_to_char_array (vector<string>& parts) {
    char** result = new char* [parts.size() + 1]; // add 1 for the NULL pointer at the end
    for (int i = 0; i < parts.size(); i++) {
        result[i] = (char*) parts[i].c_str();
    }
    result[parts.size()] = NULL;
    return result;
}


int main () {

    dup2(0,10);
    vector<int> bgs;
    
    while (true) { // repeat this loop until the user presses Ctrl + C
        dup2(10,0);

        for (int i = 0; i < bgs.size(); i++) {
            if (waitpid(bgs[i],0,WNOHANG) == bgs[i]) {
                cout << "Process: " << bgs[i] << " ended" << endl;
                bgs.erase(bgs.begin()+i);
                i--;
            }
        }
        
        // char* username = getlogin();
        char* username = getenv("USER");
        time_t now = time(0);
        string dt = string(ctime(&now));
        dt = dt.substr(0, dt.size()-1);
        cout << username << ' ' << dt << " $ ";
        
        string inputline;
        getline(cin, inputline); 

        if (inputline == string("exit")) {
            cerr << "All done, Bye!!" << endl;
            exit(1);
            break;
        }

        bool bg = false;
        inputline = trim(inputline);
        if (inputline[inputline.size()-1] == '&') {
            cout <<  "Background process found" << endl;
            bg = true;
            inputline = inputline.substr(0,inputline.size()-1);
        }

        if (inputline.substr(0,4) == "echo") {
            inputline = inputline.substr(6,inputline.size()-7);          
            cout << inputline << endl;
            continue;
        }

        vector<string> pparts = split(inputline, "|");
        for (int i = 0; i < pparts.size(); i++) {
            inputline = pparts[i];
            
            int fds[2];
            pipe(fds);
            int pid = fork();      
            if (pid == 0) {

                if (i < pparts.size()-1) {
                    dup2(fds[1],1);
                }    

                if (trim(inputline).find("cd") == 0) {
                    string dirname = trim(split(inputline, " ")[1]);
                    chdir(dirname.c_str());
                    continue;
                }             

                int pos = inputline.find('>');
                if (pos >= 0) {
                    string command = trim(inputline.substr(0,pos));
                    string filename = trim(inputline.substr(pos+1));

                    inputline = command;
                    int fd = open(filename.c_str(), O_WRONLY|O_CREAT, S_IWUSR|S_IRUSR);
                    dup2(fd,1);
                    close(fd);
                    // output redirect
                }

                pos = inputline.find('<');
                if (pos >= 0) {
                    string command = trim(inputline.substr(0,pos));
                    string filename = trim(inputline.substr(pos+1));

                    inputline = command;
                    int fd = open(filename.c_str(), O_RDONLY|O_CREAT, S_IWUSR|S_IRUSR);
                    dup2(fd,0);
                    close(fd);
                    // input redirect
                }

                vector<string> parts = split(inputline);
                char** args = vec_to_char_array(parts);
                execvp(args[0], args);
            } else {
                if (!bg) {
                    waitpid(pid,0,0);
                    dup2(fds[0],0);
                    close(fds[1]); 
                } else {
                    bgs.push_back(pid);
                }    
            }
        }
    }
}
    