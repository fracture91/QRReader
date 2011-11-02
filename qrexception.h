#pragma once

#include <ostream>
#include <sstream>


//use the following macros to create and throw QRException objects
#define QRErrCheckStdError(RESULT, FUNCNAME) \
if((RESULT) == -1) \
throw new QRException(__LINE__, errno, (FUNCNAME), strerror(errno));

#define QRErrCheckNotZero(RESULT, FUNCNAME) \
if((RESULT) != 0) \
throw new QRException(__LINE__, errno, (FUNCNAME));

class QRException {
	private:
		std::ostringstream message;
	public:
		//Make exception object given error code, name of failed function, and optionally an additional message
		QRException(int line, int code, const char *funcName, const char *_message = NULL) {
			message<<funcName<<" failed on line "<<line<<".\nError code: "<<code<<".\n";
			//if message supplied, add it to logged message
			if(_message) {
				message<<"Error message: "<<_message<<"\n";
			}
		}
		//print error message to given stream
		void printError(std::ostream &dest) {
			dest<<message.str()<<std::endl;
		}
};