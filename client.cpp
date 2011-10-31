//client
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
using namespace std;

int sendFile(char *fileName, int fdSock);
int rcvResponse(int fdSock);

int main(int argc, char *argv[]) {
	int status = 0;
	int fdSock = 0;
	struct addrinfo *addr = NULL;
	//get command line args, check for valid
	status = getaddrinfo("127.0.0.1", "2011", NULL, &addr);
	if(status != 0) {
		//print error msg
		return -1;
	}
		
	//connect to server
	fdSock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if(fdSock == -1) {
		//print error msg (errno)
		return -1;
	}
	status = connect(fdSock, addr->ai_addr, addr->ai_addrlen);
	if(status == -1) {
		//print error msg (errno)
		return -1;
	}

	//get file name
	//send file
	status = sendFile(NULL, fdSock);
	if(status != 0) {
	
	}
	cout << "client send status: " << status << endl;
	
	status = rcvResponse(fdSock);
	if(status != 0) {
	
	}
	
	//receieve return code
	//receive string
		//get string array size
		//get array
	//if success:
		//display URL
	//if failure:
		//display error message
	//repeat or break out of loop
	//disconnect
	return status;
}

int sendFile(char *fileName, int fdSock) {
	char buffer[128] = "Hello Server!";
	int bufSize = strlen(buffer)+1;
	int status = 0;
	//open file
	//read file into buffer
	//get file size
	//send file size to server
	//send file to server
	
	
	status = write(fdSock, buffer, bufSize);
	if(status == -1) {
		//print error msg (errno)
		return -1;
	}
	else
		status = 0;
	
	return status;
}

int rcvResponse(int fdSock) {
	int status = 0;
	int bufSize = 128;
	char *buffer = new char[bufSize];
	
	status = read(fdSock, buffer, bufSize);
	
	if(status == -1) {
		cout << "read error: " << strerror(errno) << endl;
		status = -1;
	}
	else {
		cout<<"response to client: "<<buffer<<endl;
	}
	
	return status;
}