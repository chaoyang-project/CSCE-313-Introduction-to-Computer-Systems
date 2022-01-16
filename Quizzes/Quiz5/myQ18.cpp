#include<stdio.h> 
#include<string> 
#include<string.h> 
#include<stdlib.h> 
#include<sys/socket.h>
#include<errno.h> 
#include<netdb.h>	
#include<arpa/inet.h>

using namespace std;


int main(int argc , char *argv[])
{
	
	char *hostname = argv[1];
    char *condition = argv[2];
	char ip[100];
	

	struct hostent *he;
	struct in_addr **addr_list;
	int i;
		
	if ( (he = gethostbyname( hostname ) ) == NULL) 
	{
		// get the host info
		herror("gethostbyname");
		return 1;
	}

	addr_list = (struct in_addr **) he->h_addr_list;
	
	for(i = 0; addr_list[i] != NULL; i++) 
	{
		strcpy(ip , inet_ntoa(*addr_list[i]) );
        string str = ip;
        if (str.find(condition) != string::npos) {
            printf("%s", ip);
            printf("\n");            
        }

	}
	
}
