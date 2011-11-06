#pragma once

#include <ostream>
#include <sstream>

#define ERR_TIMESTAMP_LEN		24

//use the following macros to create and throw QRException objects
#define QRErrCheckStdError(RESULT, FUNCNAME) \
if((RESULT) == -1) \
throw new QRException(__LINE__, errno, (FUNCNAME), strerror(errno));

#define QRErrCheckNotZero(RESULT, FUNCNAME) \
if((RESULT) != 0) \
throw new QRException(__LINE__, errno, (FUNCNAME));

#define QRAssert(CONDITION, FUNCNAME, MESSAGE) \
if(!(CONDITION)) \
throw new QRException(__LINE__, 0, (FUNCNAME), (MESSAGE));

class QRException {
	private:
		std::ostringstream message;
	public:
		//Make exception object given error code, name of failed function, and optionally an additional message
		QRException(int line, int code, const char *funcName, const char *_message = NULL) {
			message<<"Error location: "<<funcName<<" failed on line "<<line<<".\nError code: "<<code<<".\n";
			//if message supplied, add it to logged message
			if(_message) {
				message<<"Error message: "<<_message<<"\n";
			}
		}
		//print error message to given stream
		void printError(std::ostream &dest) {
			char timestamp[ERR_TIMESTAMP_LEN] = "";
			time_t timeSt;
			
			timeSt = time(NULL);
			//get timestamp
			strftime(timestamp, ERR_TIMESTAMP_LEN, "%Y-%m-%d %H:%M:%S", localtime(&timeSt));

			dest<<"Error time: "<<timestamp<<"\n"<<message.str()<<std::endl;
			dest.flush();
		}
};