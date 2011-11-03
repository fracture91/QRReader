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
#include <ctime>
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
#define TIMESTAMP_LEN		24

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

//given a file descriptor for connection with client,
// give back buffer and buffer size (caller needs to free buffer when done)
void readClientMsg(int fdConn, uint8_t **buffer, uint32_t *bufSize);

//given an image and image size, give back a string contained in QR code
void readQRCode(uint32_t imageSize, uint8_t *image, string &result);

//Given a file descriptor and result string, send result string to client
void sendQRResult(int fdConn, string result);

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
	uint8_t *imgBuf;
	uint32_t imgBufSize = 0;
	string QRResult;
	uint32_t responseStatus = 0;
	
	try {
		//fill in hints struct
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;  //don't care whether it's IPv4 or v6
		hints.ai_socktype = SOCK_STREAM; //TCP
		hints.ai_flags = AI_PASSIVE;
		
		//get command line args, check for valid
		status = getaddrinfo(NULL, g_port, &hints, &res);
		QRErrCheckNotZero(status, "getaddrinfo");
		
		//open admin log file
		
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

		//when receive new connection:
			//check for too many users. If too many, don't fork
			//fork
			//original process waits for new connection
		
		
		//new process:
		//check for rate limit violation or timeout
		//rcv client message
		readClientMsg(fdConn, &imgBuf, &imgBufSize);
		//call library to convert qr code to URL
		//determine return code
		readQRCode(imgBufSize, imgBuf, QRResult);
		//respond to client
		sendQRResult(fdConn, responseStatus, QRResult);
		
	}
	catch (QRException *exp) {
		exp->printError(cerr);
		status = -1;
	}
	return status;
}



	
void writeToAdminLog(ostream &outfile, char *ipClient, char *message) {
	char timestamp[TIMESTAMP_LEN] = "";
	
	QRAssert((ipClient != NULL), "writeToAdminLog", "NULL passed as ipClient");
	QRAssert((message != NULL), "writeToAdminLog", "NULL passed as message");
	
	//get timestamp
	strftime(timestamp, TIMESTAMP_LEN, "%Y-%m-%d %H:%M%S", localtime(time(NULL)));
	
	outfile<<timestamp<<" "<<ipClient<<" "<<message<<"\n"<<endl;
	QRAssert((!outfile.fail()), "ostream::operator<<", "Unable to write to administrative log");
	
	return;
}

void readClientMsg(int fdConn, uint8_t **buffer, uint32_t *bufSize) {
	uint32_t lenFromClient;
	int status = 0;
	QRAssert((buffer != NULL), "readClientMsg", "NULL passed as buffer");
	QRAssert((bufSize != NULL), "readClientMsg", "NULL passed as bufSize");
	
	//get buffer size
	int status = read(fdConn, &lenFromClient, sizeof(lenFromClient));
	QRErrCheckStdError(status, "read");
	lenFromClient = ntohl(lenFromClient);
	*bufSize = lenFromClient;
	
	//allocate buffer
	*buffer = (char*)calloc(lenFromClient, sizeof(char));
	QRAssert((*buffer != NULL), "calloc", "Unable to allocate memory for image buffer");
	
	//get buffer contents
	status = read(fdSock, *buffer, lenFromClient);
	QRErrCheckStdError(status, "read");
	
	return;
}

void readQRCode(uint32_t imageSize, int8_t *image, string &result) {
	int status;
	pid_t currPid;
	streamsize count = 0;
	char result_buffer[RESULT_BUF_SIZE] = "";
	char image_filename[TMP_FILENAME_SIZE] = "";
	char result_filename[TMP_FILENAME_SIZE] = "";
	char sysCall[LIB_CALL_SIZE] = "java -cp javase.jar:core.jar com.google.zxing.client.j2se.CommandLineRunner";
	fstream tmpFile;
	
	QRAssert((image != NULL), "readQRCode", "NULL passed as image");
	
	//image filename will be pid (in hex) (each process only handles 1 image at a time)
	currPid = getpid();
	sprintf(image_filename, "%x", currPid);
	sprintf(result_filename, "%x_result", currPid);
	
	//save image to file
	tmpFile.open(image_filename, fstream::out | fstream::binary);
	QRAssert(tmpFile.is_open(), "fstream::open", "Unable to open image file for writing");
	tmpFile.write((char*)image, (streamsize)imageSize);
	QRAssert((!tmpFile.fail()), "fstream::write", "Image file stream reports failure");
	tmpFile.close();
	
	//call library on file
	//system call: "java [params] [image file] > [result file]"
	sprintf(sysCall, "%s %s > %s", sysCall, image_filename, result_filename);
	status = system(sysCall);
	QRAssert((status != -1), "system", "Error calling system");
	
	//get result from result file
	tmpFile.open(result_filename, fstream::in);
	QRAssert(tmpFile.is_open(), "fstream::open", "Unable to open QR result file for reading");
	do {
		tmpFile.read(result_buffer, RESULT_BUF_SIZE);
		QRAssert((!tmpFile.fail()), "fstream::read", "Result buffer stream reports failure");
		count = tmpFile.gcount();
		//don't append if read nothing
		if(count)
			result.append(result_buffer, count);
	} while(count == RESULT_BUF_SIZE); //repeat while buffer is completely full from last read
	tmpFile.close();
	
	//delete temporary files
	remove(image_filename);
	remove(result_filename);
	return;
}

//Send the response to the client:
//Bytes 0-3: uint32 containing return code
//Bytes 4-7: uint32 containing length of result string
//Bytes 8+: result string
void sendQRResult(int fdConn, uint32_t retCode, string &result) {
	int status = 0;
	uint32_t length;
	
	//send return code (in binary, network order)
	retCode = htonl(retCode);
	status = write(fdConn, (void*)(&retCode), sizeof(retCode));
	QRErrCheckStdError(status, "write");
	
	//send length (in binary, network order)
	length = htonl(result.size());
	status = write(fdConn, (void*)(&length), sizeof(length));
	QRErrCheckStdError(status, "write");
	
	//send result string
	status = write(fdConn, result.c_str(), result.size());
	QRErrCheckStdError(status, "write");
	
	return;
}