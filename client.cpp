//client
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
using namespace std;

#include "qrexception.h"

int sendFile(char *fileName, int fdSock);
int rcvResponse(int fdSock);

int main(int argc, char *argv[]) {
	int status = 0;
	int fdSock = 0;
	struct addrinfo *addr = NULL;
	
	try	{
		//get command line args, check for valid
		status = getaddrinfo("127.0.0.1", "2011", NULL, &addr);
		QRErrCheckNotZero(status, "getaddrinfo");
			
		//connect to server
		fdSock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		QRErrCheckStdError(fdSock, "socket");
		status = connect(fdSock, addr->ai_addr, addr->ai_addrlen);
		QRErrCheckStdError(status, "connect");

		//get file name
		//send file
		status = sendFile(NULL, fdSock);
		QRErrCheckNotZero(status, "sendFile");
		
		status = rcvResponse(fdSock);
		QRErrCheckNotZero(status, "rcvResponse");
		
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
		status = close(fdSock);
		QRErrCheckStdError(status, "close");
	}
	catch (QRException *ex) {
		ex->printError(std::cerr);
		status = -1;
	}
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
	QRErrCheckStdError(status, "write");
	
	return status;
}

int rcvResponse(int fdSock) {
	int status = 0;
	int bufSize = 128;
	char *buffer = new char[bufSize];
	
	status = read(fdSock, buffer, bufSize);
	QRErrCheckStdError(status, "read");
	
	cout<<"response to client: "<<buffer<<endl;
	
	return status;
}