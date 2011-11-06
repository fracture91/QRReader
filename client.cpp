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

int sendFile(const char *fileName, int fdSock);
int rcvResponse(int fdSock);

int main(int argc, char *argv[]) {
	int status = 0;
	int fdSock = 0;
	struct addrinfo *addr = NULL;
	
	try	{
		//todo: port and IP address options
		if(argc < 2) {
			cerr << "Usage: client.elf IMAGE_FILE1, IMAGE_FILE2, ..." << endl;
			throw new QRException(__LINE__, NULL, "user input",
				"At least one image file argument is required.");
		}
		
		status = getaddrinfo("127.0.0.1", "2011", NULL, &addr);
		QRErrCheckNotZero(status, "getaddrinfo");
			
		//connect to server
		fdSock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		QRErrCheckStdError(fdSock, "socket");
		status = connect(fdSock, addr->ai_addr, addr->ai_addrlen);
		QRErrCheckStdError(status, "connect");

		//send all of the image files and receive responses from server
		for(int i = 1; i < argc; i++) {
			status = sendFile(argv[i], fdSock);
			QRErrCheckStdError(status, "sendFile");
			
			status = rcvResponse(fdSock);
			QRErrCheckStdError(status, "rcvResponse");
		}
		
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

//read filename into a uint8_t array, make *output point to said array
//returns the length of the file
uint32_t readFile(const char *fileName, uint8_t **output) {
	uint32_t length = 0;
	//ate flag makes it start at the end of the file, so we can get the size easily
	ifstream file(fileName, ifstream::binary | ifstream::in | ifstream::ate);
	if(file.is_open()) {
		length = file.tellg();
		file.seekg(0, ifstream::beg);
		*output = new uint8_t[length];
		file.read((char *)*output, length);
		file.close();
	}
	else {
		throw new QRException(__LINE__, NULL, "ifstream", "File was not opened properly");
	}
	return length;
}

int sendFile(const char *fileName, int fdSock) {
	int status = 0;
	
	uint8_t *file;
	//a plain unsigned int isn't guaranteed to be 32 bits, but uint32_t is
	uint32_t fileLength = readFile(fileName, &file);
	if(fileLength < 1) {
		throw new QRException(__LINE__, NULL, "readFile", "File length is less than 1 byte");
		}
	
	uint32_t netFileLength = htonl(fileLength);
	status = write(fdSock, &netFileLength, sizeof(netFileLength));
	QRErrCheckStdError(status, "write");
	
	status = write(fdSock, file, fileLength);
	QRErrCheckStdError(status, "write");
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

	bool printable = true;
	ostream *outputStream = &cerr;
	string statusMessage = "";
	
	switch(returnCode) {
	case 2:
		statusMessage = "Timeout: ";
		break;
	case 3:
		statusMessage = "Rate Limit Exceeded: ";
		break;
	//No returnCode 4 in the case of maximum users being hit.
	//Connection just dies in that case for efficiency's sake.
	case 0:
		outputStream = &cout;
		break;
	default:
		printable = false;
		cerr << "Failure" << endl;
	}
	
	if(printable) {
		//all ASCII strings must be null terminated, so all intelligible messages
		//must be more than 1 byte in length
		if(length > 1) {
			uint8_t *buffer = new uint8_t[length];
			status = read(fdSock, buffer, length);
			QRErrCheckStdError(status, "read");
			
			*outputStream << statusMessage << buffer << endl;
			
			delete[] buffer;
		}
		else {
			*outputStream << statusMessage << "[Message length is less than one]" << endl;
		}
	}
	
	return status;
}