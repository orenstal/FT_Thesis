/*
 * client.hh
 *
 *  Created on: Dec 28, 2016
 *      Author: Tal
 */

#ifndef TCP_SERVER_CLIENT_HH_
#define TCP_SERVER_CLIENT_HH_

#include <ctype.h>
#include <netinet/in.h>
#include<sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>


#define DEBUG false
#define DEBUG_STDOUT(x) //x

//#ifdef DEBUG
//	#define DEBUG_STDOUT(x) x
//#else
//	#define DEBUG_STDOUT(x)
//#endif


#define SERVER_BUFFER_SIZE_WITHOUT_PREFIX 300000
#define NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX 7
#define NUM_OF_DIGITS_FOR_COMMAND_PREFIX 1
#define SERVER_BUFFER_SIZE SERVER_BUFFER_SIZE_WITHOUT_PREFIX+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_COMMAND_PREFIX
#define NUM_OF_DIGITS_FOR_RET_VAL_STATUS 1
#define MAX_RET_VAL_LENGTH 300000

using namespace std;

class Client {
private:
	int checkLength (char* num, int length, int minimalExpectedValue);
	int receiveMsgFromServer (int serverSockfd, int totalReceivedBytes, char* msg, int maximalReceivedBytes);
	void readCommonServerResponse(int sockfd, char* msg, int* msgLen, int* status);
	bool sendMsg(char* serialized, int len);
protected:
	struct sockaddr_in sock_addr_server;
	int sockfd;

	virtual void serializeObject(int command, void* obj, char* serialized, int* len);
	virtual void handleReturnValue(int status, char* retVal, int len, int command, void* retValAsObj);

public:
	Client(int port, char* address);
	void connectToServer();
	void prepareToSend(void* obj, char* serialized, int* len, int command);
	bool sendMsgAndWait(char* serialized, int length, int command, void* retValAsObj);
};


#endif /* TCP_SERVER_CLIENT_HH_ */
