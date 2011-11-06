//server

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <netinet/in.h>
#include <iostream>
#include <fstream>
#include <string>
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

#define CODE_SUCCESS		0
#define CODE_FAIL			1
#define CODE_TIMEOUT		2
#define CODE_RATELIM		3

#define CONN_QUEUE			4
#define MAX_FILESIZE		0x100000

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

//connection-specific variables
int g_fdConn;
char g_ipAddrStr[16] = "";
ofstream g_AdminLogFile;

/***********************
 * Function Prototypes *
 ***********************/

//case-insensitive string compare
int cistrcmp(const char *str1, const char *str2);

//read command line arguments and set global variables
void readArgs(int argc, char *argv[]);

//handle alarm signal
void alarmHandler(int signum);

//write a message to the admin log
void writeToAdminLog(const char *message);

//given a file descriptor for connection with client,
// give back buffer and buffer size (caller needs to free buffer when done)
int readClientMsg(int fdConn, uint8_t **buffer, uint32_t *bufSize);

//given an image and image size, give back a string contained in QR code
void readQRCode(uint32_t imageSize, uint8_t *image, string &result);

//Given a file descriptor and result string, send result string to client
void sendQRResult(int fdConn, uint32_t retCode, string &result);

//Get the URI from the output of zxing call
int extractURI(const string &result, string &uri);



/************************
 * Function Definitions *
 ************************/
 
int main(int argc, char *argv[]) {
	int status = 0;
	int fdSock = 0; //file descriptor of listening socket
	int pid;
	int childIdx;
	int childStatus;
	struct addrinfo hints, *res;
	struct sockaddr_storage remoteAddr; //client IP address structure
	bool userLimitExceeded;
	uint32_t clientIP;
	socklen_t remAddrLen = sizeof(remoteAddr);
	uint8_t *imgBuf; //buffer containing image
	uint32_t imgBufSize = 0; //size of image buffer
	string QRResult; //result from QR library
	string URI = ""; //URI to send to client
	uint32_t responseStatus = 0; //status code to send to client
	
	try {
		readArgs(argc, argv);
		
		g_childPids = (int*)calloc(g_maxUsers, sizeof(int));
		
		//open admin log file: output, append
		g_AdminLogFile.open("adminLog.txt", fstream::out | fstream::app);
		writeToAdminLog("Server starting...");
		
		//fill in hints struct
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_INET;  //force IPv4 in order to comply with administrative log specification
		hints.ai_socktype = SOCK_STREAM; //TCP
		hints.ai_flags = AI_PASSIVE; //AI_PASSIVE to allow us to listen and accept connections
		
		//use hints struct to create addrinfo struct
		status = getaddrinfo(NULL, g_port, &hints, &res);
		QRErrCheckNotZero(status, "getaddrinfo");
		
		//open socket
		fdSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		
		//bind program to socket
		status = bind(fdSock, res->ai_addr, res->ai_addrlen);
		QRErrCheckStdError(status, "bind");
		
		//listen on socket
		status = listen(fdSock, CONN_QUEUE);
		QRErrCheckStdError(status, "listen");
		
		writeToAdminLog("Listening for clients");
		
		//when receive new connection:
		while((g_fdConn = accept(fdSock, (struct sockaddr *)&remoteAddr, &remAddrLen)) != -1) {
			//get IP address string (for admin log)
			clientIP = ((struct sockaddr_in *) &remoteAddr)->sin_addr.s_addr;
			sprintf(g_ipAddrStr, "%u.%u.%u.%u",
				(clientIP & 0xFF000000)>>24, (clientIP & 0xFF0000)>>16,
				(clientIP & 0xFF00)>>8,      (clientIP & 0xFF));
			
			writeToAdminLog("Client connected.");
			
			//check for too many users. If too many, don't fork
			userLimitExceeded = true;
			for(childIdx = 0; childIdx < g_maxUsers; childIdx++) {
				//no child in this slot
				if(g_childPids[childIdx] == 0) {
					userLimitExceeded = false;
					break;
				}
				else {
					//check on status of child
					status = waitpid(g_childPids[childIdx], &childStatus, WNOHANG);
					//child does not exist or has exited
					if(status == -1 || status == g_childPids[childIdx]) {
						g_childPids[childIdx] = 0;
						userLimitExceeded = false;
						break;
					}
				}
			}
			//disconnect client if user limit exceeded
			if(userLimitExceeded) {
				close(g_fdConn);
				g_fdConn = 0;
				writeToAdminLog("Client dropped: User limit exceeded.");
				continue;
			}
			
			//fork process; child process will handle client connection, parent will keep listening
			pid = fork();
			QRErrCheckStdError(pid, "fork");
			QRAssert(pid >= 0, "fork", "Unexpected return value");
			//put child pid in children list
			g_childPids[childIdx] = pid;
			if(pid > 0) {
				//original process waits for new connection
				continue;
			}
			//child process
			else if(pid == 0) {
				//register timeout signal handler
				signal(SIGALRM, alarmHandler);
#if DEBUG
				cout<<"server: Accepted Connection" << endl;
				cout<<"Server: Client Address: "<<g_ipAddrStr<<endl;
#endif
				//while not timed out:
				do {
					responseStatus = 0;
					URI.clear();
					QRResult.clear();
					status = readClientMsg(g_fdConn, &imgBuf, &imgBufSize);
					//set alarm for timeout
					alarm(g_timeOut);
					//check to make sure file is not too large
					if(status == 1) {
						writeToAdminLog("File too large; ignoring request.");
					}
					else if(status == 0) {
						writeToAdminLog("Receiving file from client");
						
						//TODO: check for rate limit violation
						
						//call library to convert qr code to text
						readQRCode(imgBufSize, imgBuf, QRResult);
						free(imgBuf);
						imgBuf = NULL;
						imgBufSize = 0;
						
						//extract URI from result from library
						status = extractURI(QRResult, URI);
						//error reading qr code
						if(status == 1) {
							writeToAdminLog("Invalid QR code (unable to parse)");
							string errMsg = "";
							sendQRResult(g_fdConn, CODE_FAIL, errMsg);
						}
						else {
							writeToAdminLog("Sending QR result to client.");
							writeToAdminLog(URI.c_str());
							sendQRResult(g_fdConn, responseStatus, URI);
						}
					}
					else if(status < 0) {
						writeToAdminLog("Disconnecting from client.");
						close(g_fdConn);
						g_fdConn = 0;
					}
				} while(g_fdConn > 0);
				return 0;
			}
		}
		QRErrCheckStdError(g_fdConn, "accept");
	}
	catch (QRException *exp) {
		exp->printError(cerr);
		status = -1;
	}
	free(imgBuf);
	imgBuf = NULL;
	free(g_childPids);
	g_AdminLogFile.close();
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

//Read command-line arguments, set global variables
//Input:
//	argc: number of arguments (as passed to main)
//	argv: array of character arrays containing arguments (as passed to main)
void readArgs(int argc, char *argv[]) {
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
}

//Writes a message to the admin log
//Input:
//	message: message to write to log
void writeToAdminLog(const char *message) {
	char timestamp[TIMESTAMP_LEN] = "";
	time_t timeSt;
	
	QRAssert((message != NULL), "writeToAdminLog", "NULL passed as message");
	
	timeSt = time(NULL);
	//get timestamp
	strftime(timestamp, TIMESTAMP_LEN, "%Y-%m-%d %H:%M%S", localtime(&timeSt));
	
	g_AdminLogFile<<timestamp<<" "<<g_ipAddrStr<<" "<<message<<"\n"<<endl;
	QRAssert((!g_AdminLogFile.fail()), "ostream::operator<<", "Unable to write to administrative log");
	
	return;
}

void alarmHandler(int signum) {
	string errMsg = "timeout";
	sendQRResult(g_fdConn, CODE_TIMEOUT, errMsg);
	close(g_fdConn);
	g_fdConn = 0;
}

//read message from client
//Notes: caller needs to free buffer
//Input:
//	fdConn: file descriptor for socket to read from
//Output:
//	buffer: pointer to unallocated buffer. This buffer is allocated and the file data placed into it
//	bufSize: size of buffer in bytes
//	return value: 1 if file is over size limit, 0 on success, <0 on failure or end of input
int readClientMsg(int fdConn, uint8_t **buffer, uint32_t *bufSize) {
	uint32_t lenFromClient;
	int status = 0;
	bool tooLarge = false;
	QRAssert((buffer != NULL), "readClientMsg", "NULL passed as buffer");
	QRAssert((bufSize != NULL), "readClientMsg", "NULL passed as bufSize");
	
	if(fdConn == 0) {
		return -1;
	}
	
	//get buffer size
	status = read(fdConn, &lenFromClient, sizeof(lenFromClient));
	if(status < 0) {
		return status;
	}
	
	//check for no more data
	if(status == 0) {
		return -1;
	}
	
	lenFromClient = ntohl(lenFromClient);
	*bufSize = lenFromClient;
	
	
	//check for file too large
	if(lenFromClient > MAX_FILESIZE) {
		tooLarge = true;
	}
	//allocate buffer
	*buffer = (uint8_t*)calloc(lenFromClient, sizeof(uint8_t));
	QRAssert((*buffer != NULL), "calloc", "Unable to allocate memory for image buffer");
	
	//get buffer contents
	status = read(fdConn, *buffer, lenFromClient);
	QRErrCheckStdError(status, "read");
	
	if(tooLarge) {
		free(*buffer);
		*buffer = 0;
		*bufSize = 0;
		return 1;
	}
	
	return 0;
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

//Assign the URI contained in the result string to the uri string if it can be found.
//If the result is not identified as a URI or there are other problems,
//this function returns 1.  Otherwise, 0.
int extractURI(const string &result, string &uri) {
	istringstream resultStream(result);
	string line;
	getline(resultStream, line);
	
	if(line.find("type: URI") != string::npos) {
		string target = "Parsed result:\n";
		size_t targetLocation = result.find(target);
		if(targetLocation != string::npos) {
			resultStream.seekg(targetLocation + target.length());
			getline(resultStream, line);
			if(!line.empty()) {
				uri.assign(line);
				return 0;
			}
		}
	}
	
	return 1;
}

//Send the response to the client:
//Input:
//	fdConn: file descriptor for socket to send response through
//	retCode: return code
//	result: string containing URI to send to client
void sendQRResult(int fdConn, uint32_t retCode, string &result) {
	int status = 0;
	uint32_t strLen;
	uint8_t *message;
	int msgLen;
	
	if(result.size() == 0) {
		msgLen = 8;
	}
	else {
		msgLen = 9+result.size();
	}
	message = (uint8_t*)calloc(msgLen, sizeof(uint8_t));
	
	//Bytes 0-3: uint32 containing return code
	retCode = htonl(retCode);
	memcpy((message+0), &retCode, 4);
	
	//Bytes 4-7: uint32 containing length of result string
	strLen = htonl(result.size() + 1);
	memcpy((message+4), &strLen, 4);
	
	//Bytes 8+: result string
	if(result.size() > 0)
	{
		memcpy((message+8), result.c_str(), result.size()+1);
	}
	
	status = write(fdConn, message, msgLen);
	QRErrCheckStdError(status, "write");
	
	return;
}