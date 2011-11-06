Author:				Eric Finn, Andrew Hurle
Date:				2011-11-05
Project ID:			Project 1
CS Class:			CS 3516
Programming Language:		C++, but it feels like C
OS/Hardware dependencies:	Linux, a network interface card

Problem Description:		Write a server and client. Client will send an QR code to server as .png, server will respond with URL found in QR code.

Program Assumptions and Restrictions:
	The program assumes a stable connection between the client and server. If the connection is interrupted, the program fails.
	The program assumes that the user starting the server knows how to use the command line arguments.
	The program assumes that concurrently accessing log files doesn't make things break.
	The max users option can be flaky.
	The file size limit is 1 MB.
	The QR code is assumed to be a URI. If it is not, the server reports a failure to the client.

Interfaces:
	User
		Command line arguments
		Log files
	File
		File system:
			server creates temporary files (and deletes them too)
			server creates an admin log file and an error log file
			client reads files passed as command line parameters

How to build the program:
	1. make
	alternatively:
	1. make clean
	2. make

References:
	http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html - general guide to 
	http://linux.die.net/
	http://cplusplus.com/
	http://www.kernel.org/doc/man-pages/online/pages/man3/getaddrinfo.3.html