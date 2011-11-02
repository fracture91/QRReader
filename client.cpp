//client
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <stdint.h>
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
		QRErrCheckStdError(status, "sendFile");
		
		status = rcvResponse(fdSock);
		QRErrCheckStdError(status, "rcvResponse");
		
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

//read filename into a char array, make *output point to said array
//returns the length of the file
uint32_t readFile(char *fileName, char **output) {
	uint32_t length = 0;
	//ate flag makes it start at the end of the file, so we can get the size easily
	ifstream file(fileName, ifstream::binary | ifstream::in | ifstream::ate);
	if(file.is_open()) {
		length = file.tellg();
		file.seekg(0, ifstream::beg);
		*output = new char[length];
		file.read(*output, length);
		file.close();
	}
	else {
		throw new QRException(__LINE__, NULL, "ifstream", "File was not opened properly");
	}
	return length;
}

int sendFile(char *fileName, int fdSock) {
	int status = 0;
	
	char *file;
	//a plain unsigned int isn't guaranteed to be 32 bits, but uint32_t is
	uint32_t fileLength = readFile(".gitignore", &file);
	if(fileLength < 1) {
		throw new QRException(__LINE__, NULL, "readFile", "File length is less than 1 byte");
		}
	uint32_t netFileLength = htonl(fileLength);
	int messageLength = fileLength + 4;
	char *buffer = new char[messageLength];
	memcpy(buffer, &netFileLength, 4);
	memcpy(buffer+4, file, fileLength);
	
	status = write(fdSock, buffer, messageLength);
	QRErrCheckStdError(status, "write");
	delete[] buffer;
	delete[] file;
	
	return status;
}

uint32_t readIntFromResponse(int fdSock) {
	uint32_t netNumber = 0;
	int status = read(fdSock, &netNumber, 4);
	QRErrCheckStdError(status, "read");
	
	return ntohl(netNumber);
}

int rcvResponse(int fdSock) {
	int status = 0;
	uint32_t returnCode = readIntFromResponse(fdSock);
	uint32_t length = readIntFromResponse(fdSock);

	if(returnCode == 0 || returnCode == 2) {
		if(returnCode == 2) {
			cerr << "Timeout: ";
		}
		
		char *buffer = new char[length];
		status = read(fdSock, buffer, length);
		QRErrCheckStdError(status, "read");
		
		if(returnCode == 0) {
			cout << buffer << endl;
		}
		else {
			cerr << buffer << endl;
		}
		
		delete[] buffer;
	}
	else {
		cerr << "Failure" << endl;
	}
	
	return status;
}