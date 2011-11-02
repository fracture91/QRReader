//server
#define DEBUG 1

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <iostream>
#include <fstream>
using namespace std;

#include "qrexception.h"

/***********
 * Defines *
 ***********/
#define RESULT_BUF_SIZE     512
#define TMP_FILENAME_SIZE   64
#define LIB_CALL_SIZE       128

/********************
 * Global Variables *
 ********************/

char g_port[16] = "2011";
int g_rateReqs = 0;
int g_rateSecs = 0;
int g_maxUsers = 0;
int g_timeOut = 0;

//Function Prototypes

//write a message to the admin log
int writeToAdminLog(char *ipClient, char *message);

//given an image and image size, give back a string contained in QR code
int readQRCode(uint32_t imageSize, int8_t *image, string &result);

int sendQRResult(string result);

//Function definitions

int main(int argc, char *argv[]) {
	int status = 0;
	int fdSock = 0;
	struct addrinfo hints, *res;
	struct sockaddr_storage remoteAddr;
	socklen_t remAddrLen = sizeof(remoteAddr);
	int fdConn = 0;
	int bufSize = 128;
	char *buffer = new char[bufSize];
	
	try {
		//fill in hints struct
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;  //don't care whether it's IPv4 or v6
		hints.ai_socktype = SOCK_STREAM; //TCP
		hints.ai_flags = AI_PASSIVE;
		
		//get command line args, check for valid
		status = getaddrinfo(NULL, g_port, &hints, &res);
		QRErrCheckNotZero(status, "getaddrinfo");
		
		//open socket
		fdSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		cout << "fdSock: " << fdSock << endl;
		
		status = bind(fdSock, res->ai_addr, res->ai_addrlen);
		QRErrCheckStdError(status, "bind");
		
		status = listen(fdSock, 10);
		QRErrCheckStdError(status, "listen");
		
		cout << "server accepting" << endl;
		fdConn = accept(fdSock, (struct sockaddr *)&remoteAddr, &remAddrLen);
		QRErrCheckStdError(fdConn, "accept");
	#if DEBUG
		cout << "server accepted" << endl;
		cout << "remAddrLen: " << remAddrLen << endl;
		cout << "fdConn value: " << fdConn << endl;
		cout << "addr data: " << ((struct sockaddr_in6 *) &remoteAddr)->sin6_addr.s6_addr << endl;
	#endif
		status = read(fdConn, buffer, bufSize);
		QRErrCheckStdError(status, "read");
	#if DEBUG
		cout << "server message: " << buffer << endl;
	#endif

		//prototype: respond to client
		strcpy(buffer, "Hello client!");
		bufSize = strlen(buffer);
		status = write(fdConn, buffer, bufSize);
		QRErrCheckStdError(status, "write");

		//when receive new connection:
			//check for too many users. If too many, don't fork
			//fork
			//original process waits for new connection
	}
	catch (QRException *exp) {
		exp->printError(cerr);
		status = -1;
	}
	return status;
}


//new process:
//check for rate limit violation or timeout
//rcv client message
	//check for file too large
//call library to convert qr code to URL
//determine return code
//respond to client
	//send return code to client
		//convert to network byte order
		//send number
	//send string len to client
		//convert to network byte order
		//send number
	//send string to client

//TODO: error checking
int readQRCode(uint32_t imageSize, int8_t *image, string &result) {
	pid_t currPid;
	streamsize count = 0;
	char result_buffer[RESULT_BUF_SIZE] = "";
	char image_filename[TMP_FILENAME_SIZE] = "";
	char result_filename[TMP_FILENAME_SIZE] = "";
	char sysCall[LIB_CALL_SIZE] = "java -cp javase.jar:core.jar com.google.zxing.client.j2se.CommandLineRunner";
	fstream tmpFile;
	
	//image filename will be pid (in hex) (each process only handles 1 image at a time)
	currPid = getpid();
	sprintf(image_filename, "%x", currPid);
	sprintf(result_filename, "%x_result", currPid);
	
	//save image to file
	tmpFile.open(image_filename, fstream::out | fstream::binary);
	tmpFile.write((char*)image, (streamsize)imageSize);
	tmpFile.close();
	
	//call library on file
	//system call: "java [params] [image file] > [result file]"
	sprintf(sysCall, "%s %s > %s", sysCall, image_filename, result_filename);
	system(sysCall);
	
	//get result from result file
	tmpFile.open(result_filename, fstream::in);
	do {
		tmpFile.read(result_buffer, RESULT_BUF_SIZE);
		count = tmpFile.gcount();
		//don't append if read nothing
		if(count)
			result.append(result_buffer, count);
	} while(count == RESULT_BUF_SIZE); //repeat while buffer is completely full from last read
	tmpFile.close();
	
	//delete temporary files
	remove(image_filename);
	remove(result_filename);
	return 0;
}
