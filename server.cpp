//server

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
#define DEBUG 1

#define RESULT_BUF_SIZE     512
#define TMP_FILENAME_SIZE   64
#define LIB_CALL_SIZE       128
#define TIMESTAMP_LEN		24

/********************
 * Global Variables *
 ********************/

//command line options, set to defaults
char g_port[16] = "2011";
int g_rateReqs = 4;
int g_rateSecs = 60;
int g_maxUsers = 2;
int g_timeOut = 180;

int *g_childPids = NULL;

/***********************
 * Function Prototypes *
 ***********************/

//case-insensitive string compare
int cistrcmp(const char *str1, const char *str2);

//write a message to the admin log
int writeToAdminLog(char *ipClient, char *message);

//given a file descriptor for connection with client,
// give back buffer and buffer size (caller needs to free buffer when done)
void readClientMsg(int fdConn, uint8_t **buffer, uint32_t *bufSize);

//given an image and image size, give back a string contained in QR code
void readQRCode(uint32_t imageSize, uint8_t *image, string &result);

//Given a file descriptor and result string, send result string to client
void sendQRResult(int fdConn, uint32_t retCode, string &result);



/************************
 * Function Definitions *
 ************************/

int main(int argc, char *argv[]) {
	int status = 0;
	int fdSock = 0; //file descriptor of listening socket
	int fdConn = 0; //file descriptor of current connection
	int pid;
	struct addrinfo hints, *res;
	struct sockaddr_storage remoteAddr; //client IP address structure
	char clientIPStr[16] = ""; //string containing client IP address
	uint32_t clientIP;
	socklen_t remAddrLen = sizeof(remoteAddr);
	uint8_t *imgBuf; //buffer containing image
	uint32_t imgBufSize = 0; //size of image buffer
	string QRResult; //result from QR library and string to send to client
	uint32_t responseStatus = 0; //status code to send to client
	ofstream adminLog; //admin log stream object
	
	try {
		//get command line arguments
		//this is unsafe and will probably segfault if the user doesn't supply args properly
		if(argc > 1) {
			for(int curArg = 1; curArg <= argc; curArg++) {
				if(!cistrcmp(argv[curArg], "port")) {
					curArg++;
					strncpy(g_port, argv[curArg], sizeof(g_port)-1);
				}
				else if(!cistrcmp(argv[curArg], "rate")) {
					curArg++;
					g_rateReqs = atoi(argv[curArg]);
					curArg++;
					g_rateSecs = atoi(argv[curArg]);
				}
				else if(!cistrcmp(argv[curArg], "max_users")) {
					curArg++;
					g_maxUsers = atoi(argv[curArg]);
				}
				else if(!cistrcmp(argv[curArg], "time_out")) {
					curArg++;
					g_timeOut = atoi(argv[curArg]);
				}
			}
		}
		
		g_childPids = (int*)calloc(g_maxUsers, sizeof(int));
		
		//fill in hints struct
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_INET;  //force IPv4 in order to comply with administrative log specification
		hints.ai_socktype = SOCK_STREAM; //TCP
		hints.ai_flags = AI_PASSIVE; //AI_PASSIVE to allow us to listen and accept connections
		
		//use hints struct to create addrinfo struct
		status = getaddrinfo(NULL, g_port, &hints, &res);
		QRErrCheckNotZero(status, "getaddrinfo");
		
		//open admin log file: output, append
		adminLog.open("adminLog.txt", fstream::out | fstream::app);
		
		//open socket
		fdSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		
		//bind socket
		status = bind(fdSock, res->ai_addr, res->ai_addrlen);
		QRErrCheckStdError(status, "bind");
		
		//listen on socket
		status = listen(fdSock, 10);
		QRErrCheckStdError(status, "listen");
		
		//when receive new connection
		while((fdConn = accept(fdSock, (struct sockaddr *)&remoteAddr, &remAddrLen)) != -1) {
			//check for too many users. If too many, don't fork
			//fork
			pid = fork();
			QRErrCheckStdError(pid, "fork");
			if(pid > 0) {
				//original process waits for new connection
				continue;
			}
			//child process
			else if(pid == 0) {
				//get IP address string (for admin log)
				clientIP = ((struct sockaddr_in *) &remoteAddr)->sin_addr.s_addr;
				sprintf(clientIPStr, "%ud.%ud.%ud.%ud",
					(clientIP & 0xFF000000)>>24, (clientIP & 0xFF0000)>>16,
					(clientIP & 0xFF00)>>8,      (clientIP & 0xFF));
#if DEBUG
				cout<<"server: Accepted Connection" << endl;
				cout<<"Server: Client Address: "<<clientIPStr<<endl;
#endif
				//rcv client message
				readClientMsg(fdConn, &imgBuf, &imgBufSize);
				//check for rate limit violation or timeout
				//call library to convert qr code to URL
				readQRCode(imgBufSize, imgBuf, QRResult);
				free(imgBuf);
				imgBuf = NULL;
				//determine return code
				//respond to client
				sendQRResult(fdConn, responseStatus, QRResult);
				return 0;
			}
		}
		QRErrCheckStdError(fdConn, "accept");
		
		
		
	}
	catch (QRException *exp) {
		exp->printError(cerr);
		status = -1;
	}
	free(imgBuf);
	imgBuf = NULL;
	free(g_childPids);
	adminLog.close();
	return status;
}

//case-insensitive string compare
int cistrcmp(const char *str1, const char *str2) {
	int len1, len2;
	char *lowstr1, *lowstr2;
	int cmpresult;
	
	QRAssert((str1 != NULL), "cistrcmp", "NULL passed as str1");
	QRAssert((str2 != NULL), "cistrcmp", "NULL passed as str2");
	
	len1 = strlen(str1)+1;
	len2 = strlen(str2)+1;
	lowstr1 = (char*)calloc(len1, sizeof(char));
	lowstr2 = (char*)calloc(len2, sizeof(char));
	
	for(int index = 0; index < len1 && index < len2; index++) {
		if(index < len1)
			lowstr1[index] = tolower(str1[index]);
		if(index < len2)
			lowstr2[index] = tolower(str2[index]);
	}
	
	cmpresult = strcmp(lowstr1, lowstr2);
	
	free(lowstr1);
	free(lowstr2);
	
	return cmpresult;
}

//Writes a message to the admin log
//Input:
//	outfile: stream to write to
//	ipClient: string containing IP address of client (pass empty string for 
//	message: message to write to log
void writeToAdminLog(ostream &outfile, char *ipClient, char *message) {
	char timestamp[TIMESTAMP_LEN] = "";
	time_t timeSt;
	
	QRAssert((ipClient != NULL), "writeToAdminLog", "NULL passed as ipClient");
	QRAssert((message != NULL), "writeToAdminLog", "NULL passed as message");
	
	timeSt = time(NULL);
	//get timestamp
	strftime(timestamp, TIMESTAMP_LEN, "%Y-%m-%d %H:%M%S", localtime(&timeSt));
	
	outfile<<timestamp<<" "<<ipClient<<" "<<message<<"\n"<<endl;
	QRAssert((!outfile.fail()), "ostream::operator<<", "Unable to write to administrative log");
	
	return;
}

//read message from client
//Notes: caller needs to free buffer
//Input:
//	fdConn: file descriptor for socket to read from
//Output:
//	buffer: pointer to unallocated buffer. This buffer is allocated and the file data placed into it
//	bufSize: size of buffer in bytes
void readClientMsg(int fdConn, uint8_t **buffer, uint32_t *bufSize) {
	uint32_t lenFromClient;
	int status = 0;
	QRAssert((buffer != NULL), "readClientMsg", "NULL passed as buffer");
	QRAssert((bufSize != NULL), "readClientMsg", "NULL passed as bufSize");
	
	//get buffer size
	status = read(fdConn, &lenFromClient, sizeof(lenFromClient));
	QRErrCheckStdError(status, "read");
	lenFromClient = ntohl(lenFromClient);
	*bufSize = lenFromClient;
	
	//allocate buffer
	*buffer = (uint8_t*)calloc(lenFromClient, sizeof(uint8_t));
	QRAssert((*buffer != NULL), "calloc", "Unable to allocate memory for image buffer");
	
	//get buffer contents
	status = read(fdConn, *buffer, lenFromClient);
	QRErrCheckStdError(status, "read");
	
	return;
}

//Read a QR code
//Input:
//	imageSize: Size of image, in bytes
//	image: Array of bytes containing image
//Output:
//	result: result of calling QR reading library on the QR code
void readQRCode(uint32_t imageSize, uint8_t *image, string &result) {
	int status;
	pid_t currPid;
	streamsize count = 0;
	char result_buffer[RESULT_BUF_SIZE] = "";
	char image_filename[TMP_FILENAME_SIZE] = "";
	char result_filename[TMP_FILENAME_SIZE] = "";
	char sysCall[LIB_CALL_SIZE] = "java -cp javase.jar:core.jar com.google.zxing.client.j2se.CommandLineRunner";
	fstream tmpFile;
	
	try {
		QRAssert((image != NULL), "readQRCode", "NULL passed as image");
		
		//image filename will be pid (in hex) (each process only handles 1 image at a time)
		currPid = getpid();
		sprintf(image_filename, "%x.png", currPid);
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
			count = tmpFile.gcount();
			//don't append if read nothing
			if(count)
				result.append(result_buffer, count);
		} while(count == RESULT_BUF_SIZE); //repeat while buffer is completely full from last read
		tmpFile.close();
	}
	
	catch (QRException *exp) {
		tmpFile.close();
		//delete temporary files
		remove(image_filename);
		remove(result_filename);
		throw exp;
	}
	remove(image_filename);
	remove(result_filename);
	return;
}

//Send the response to the client:
//Input:
//	fdConn: file descriptor for socket to send response through
//	retCode: return code
//	result: string containing URL to send to client
void sendQRResult(int fdConn, uint32_t retCode, string &result) {
	int status = 0;
	uint32_t length;
	
	//Bytes 0-3: uint32 containing return code
	retCode = htonl(retCode);
	status = write(fdConn, (void*)(&retCode), sizeof(retCode));
	QRErrCheckStdError(status, "write");
	
	//Bytes 4-7: uint32 containing length of result string
	length = htonl(result.size());
	status = write(fdConn, (void*)(&length), sizeof(length));
	QRErrCheckStdError(status, "write");
	
	//Bytes 8+: result string
	status = write(fdConn, result.c_str(), result.size());
	QRErrCheckStdError(status, "write");
	
	return;
}