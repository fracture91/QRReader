//server
#define DEBUG 1

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
using namespace std;

int writeToAdminLog(char *ipClient, char *message);

int main(int argc, char *argv[]) {
	int status = 0;
	char port[16] = "2011";
	int rateReqs = 0;
	int rateSecs = 0;
	int maxUsers = 0;
	int timeOut = 0;
	int fdSock = 0;
	struct addrinfo hints, *res;
	struct sockaddr_storage remoteAddr;
	socklen_t remAddrLen = sizeof(remoteAddr);
	int fdConn = 0;
	int bufSize = 128;
	char *buffer = new char[bufSize];
	
	//fill in hints struct
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;  //don't care whether it's IPv4 or v6
	hints.ai_socktype = SOCK_STREAM; //TCP
	hints.ai_flags = AI_PASSIVE;
	
	//get command line args, check for valid
	status = getaddrinfo(NULL, port, &hints, &res);
	if(status != 0) {
		cout << "getaddrinfo error: " << strerror(errno) << endl;
		return -1;
	}
	
	//open socket
	fdSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	cout << "fdSock: " << fdSock << endl;
	
	status = bind(fdSock, res->ai_addr, res->ai_addrlen); 
	if(status != 0) {
		cout << "bind error: " << strerror(errno) << endl;
		return -1;
	}
	
	
	status = listen(fdSock, 10);
	if(status != 0) {
		cout << "listen error: " << strerror(errno) << endl;
		return -1;
	}
	
	cout << "server accepting" << endl;
	fdConn = accept(fdSock, (struct sockaddr *)&remoteAddr, &remAddrLen);
#if DEBUG
	cout << "server accepted" << endl;
	cout << "remAddrLen: " << remAddrLen << endl;
	cout << "fdConn value: " << fdConn << endl;
	cout << "addr data: " << ((struct sockaddr_in6 *) &remoteAddr)->sin6_addr.s6_addr << endl;
#endif
	status = read(fdConn, buffer, bufSize);
#if DEBUG
	cout << "server message: " << buffer << endl;
#endif
	if(status == -1) {
		cout << "read error: " << strerror(errno) << endl;
		return -1;
	}

	//prototype: respond to client
	strcpy(buffer, "Hello client!");
	bufSize = strlen(buffer);
	status = write(fdConn, buffer, bufSize);
	if(status == -1) {
		cout << "write error: " << strerror(errno) << endl;
		return -1;
	}
	

	//when receive new connection:
		//check for too many users. If too many, don't fork
		//fork
		//original process waits for new connection
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