/*
 * client.cpp
 *
 *  Created on: Dec 28, 2016
 *      Author: Tal
 */


#include "client.hh"

using namespace std;

// todo for click usage. Comment for local interface.
#include <click/config.h>
#include <click/args.hh>
#include <click/glue.hh>
CLICK_DECLS


Client::Client(int port, char* address) {
	sock_addr_server.sin_family = AF_INET;
	sock_addr_server.sin_addr.s_addr = inet_addr(address); // = INADDR_ANY;
	sock_addr_server.sin_port = htons(port);
	memset(&(sock_addr_server.sin_zero), '\0', 8);

	sockfd = -1;
}

void Client::connectToServer() {
	//Create socket
	sockfd = socket(AF_INET , SOCK_STREAM , 0);
	if (sockfd == -1) {
//		cout << "ERROR: Could not create socket" << endl;
		printf("ERROR: Could not create socket\n");
	}

	printf("Socket created\n");
//	cout << "Socket created" << endl;

	// activate keep-alive mechanism
//	int val = 1;
//	setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof val);

	//Connect to remote server
	if (connect(sockfd , (struct sockaddr *)&sock_addr_server , sizeof(sock_addr_server)) < 0) {
//		click << "ERROR: Connect failed." << endl;
		printf("ERROR: Connect failed.\n");
		return;
	}

	printf("Connected\n");
}

void Client::serializeObject(int command, void* obj, char* serialized, int* len) {
	printf("Client::serializeObject\n");
	//	cout << "Client::serializeObject" << endl;
}

void Client::handleReturnValue(int status, char* retVal, int len, int command) {
	printf("Client::handleReturnValue\n");
}

/*
 * This function returns a string representation of length numOfDigits for the inserted number. If the number's
 * length is smaller than numOfDigits, we pad the returned string with 0 at the beginning appropriately.
 */
void intToStringDigits (int number, uint8_t numOfDigits, char* numAsStr)
{
	sprintf(numAsStr, "%0*d", numOfDigits, number);
}

void Client::prepareToSend(void* obj, char* serialized, int* len, int command) {
	printf("Client::prepareToSend\n");
	// we leave NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX (7) digits for the serialization length
	serializeObject(command, obj, serialized + NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_COMMAND_PREFIX, len);
	printf("Client::done serializing\n");

	char numAsStr[NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+1];
	intToStringDigits(*len, NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX, numAsStr);

	printf("numAsStr is: %s\n", numAsStr);
//	cout << "lenPrefix is: " << lenPrefix << endl;
	printf("len is: %d\n", *len);

	for (int i=0; i<NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX; i++) {
		printf("numAsStr[%d]: %c\n", i, numAsStr[i]);
		serialized[i] = numAsStr[i];
	}

	char commandAsStr[NUM_OF_DIGITS_FOR_COMMAND_PREFIX+1];
	intToStringDigits(command, NUM_OF_DIGITS_FOR_COMMAND_PREFIX, commandAsStr);
	printf("commandAsStr is: %s\n", commandAsStr);

	for (int i=0; i<NUM_OF_DIGITS_FOR_COMMAND_PREFIX; i++) {
		printf("commandAsStr[%d]: %c\n", i, commandAsStr[i]);
		serialized[NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX + i] = commandAsStr[i];
	}

	*len += NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_COMMAND_PREFIX;
	printf("len is: %d\n", *len);
	printf("serialized is: %s\n", serialized);
	printf("[Client::prepareToSend] Done\n");
	fflush(stdout);
}

bool Client::sendMsg(char* serialized, int length) {
	printf("in sendMsg.. length: %d, msg:", length);
	//	cout << "in sendMsg.. length: " << length << ", msg: ";

	// print message content
	for(int i=0; i< length; i++) {
		printf("%c", serialized[i]);
//		cout << serialized[i];
	}

	printf("\n");
//	cout << endl;


	int totalSentBytes = 0;

	if (length > SERVER_BUFFER_SIZE) {
		printf("ERROR: can't send message that is longer than %d\n", SERVER_BUFFER_SIZE);
//		cout << "ERROR: can't send message that is longer than " << SERVER_BUFFER_SIZE << endl;
		return false;
	}

	// Write the message to the server
	while (totalSentBytes < length) {
		printf("sending..\n");
		int ret = send(sockfd, serialized, length - totalSentBytes, 0);

		if (ret == 0) {
			printf("ERROR: The server is terminated. exit..\n");
//			cout << "ERROR: The server is terminated. exit..";
			exit(1);
		}

		if (ret < 0) {
			// trying to send one more time after failing the first time
			ret = send(sockfd, serialized, length - totalSentBytes, 0);

			if (ret == 0) {
				printf("ERROR: The server is terminated. exit..\n");
//				cout << "ERROR: The server is terminated. exit..";
				exit(1);
			}

			if (ret < 0) {
				return false;
			}
		}

		totalSentBytes += ret;
		serialized += ret;
	}

	printf("totalSentBytes: %d\n", totalSentBytes);
//	cout << "totalSentBytes: " << totalSentBytes << endl;

	return true;

}

void Client::readCommonServerResponse(int sockfd, char* msg, int* msgLen, int* status) {

	// receive message in the following format: [7 digits representing the client name's length][client name]
	int totalReceivedBytes = receiveMsgFromServer(sockfd, 0, msg, NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_RET_VAL_STATUS);
	printf("Client::readCommonServerResponse] 1: sockfd: %d, , totalReceivedBytes: %d\n", sockfd, totalReceivedBytes);
	printf("total received bytes is %d\n", totalReceivedBytes);
//	cout << "Server::readCommonServerRequest] 1: sockfd: " << sockfd << ", totalReceivedBytes: " << totalReceivedBytes << endl;

	// client socket was closed and removed
	if (totalReceivedBytes == -1) {
		*msgLen = -1;
		return;
	}

	char *ptr = msg + totalReceivedBytes;

	// convert the received length of the client name to int
	char len[NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX + 1];
	memset(len, '\0', NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX + 1);
	strncpy(len, msg, NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX);
	*msgLen = checkLength(len, NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX, 0);
	printf("msgLen is: %d, len: %s\n", *msgLen, len);
//	cout << "msgLen is: " << *msgLen << ", len: " << len << endl;

	// convert the received command digit to int
	char receivedStatus[NUM_OF_DIGITS_FOR_RET_VAL_STATUS + 1];
	memset(receivedStatus, '\0', NUM_OF_DIGITS_FOR_RET_VAL_STATUS + 1);
	strncpy(receivedStatus, msg+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX, NUM_OF_DIGITS_FOR_RET_VAL_STATUS);
	*status = checkLength(receivedStatus, NUM_OF_DIGITS_FOR_RET_VAL_STATUS, 0);
	printf("status is: %d, receivedStatus: %s\n", *status, receivedStatus);
//	cout << "status is: " << *status << ", receivedStatus: " << receivedStatus << endl;

	// receive the content
	totalReceivedBytes = receiveMsgFromServer(sockfd, totalReceivedBytes, ptr, *msgLen+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_RET_VAL_STATUS);
	printf("Client::readCommonServerResponse] 2: totalReceivedBytes: %d\n", totalReceivedBytes);
//	cout << "Server::readCommonServerRequest] 2: totalReceivedBytes: " << totalReceivedBytes << endl;

	// client socket was closed and removed
	if (totalReceivedBytes == -1) {
		return;
	}

}

/*
 * This function checks that the inserted num is actually a number, and that is at least of minimalExpectedValue.
 * length is num size. If num passed these checks - returns it as integer. Otherwise returns -1.
 */
int Client::checkLength (char* num, int length, int minimalExpectedValue) {
	printf("Client::checkLength\n");
	fflush(stdout);

	for (int i=0; i<length; i++) {
		fflush(stdout);
		if (isdigit(num[i]) == false) {
			return -1;
		}
	}

	int convertedNum = atoi(num);

	if (convertedNum < minimalExpectedValue) {
		return -1;
	}

	return convertedNum;
}

/*
 * This function handles the receiving of a message from the client clientName (with socket fd clientSockfd)
 * of length (maximalReceivedBytes - totalReceivedBytes) from totalReceivedBytes to maximalReceivedBytes.
 * It returns how many it received in here, and inserts this message to msg.
 * In case that the client disconnected abruptly - it turns the terminateItself flag to true.
 */
int Client::receiveMsgFromServer (int serverSockfd, int totalReceivedBytes, char* msg, int maximalReceivedBytes) {
	printf("Client::receiveMsgFromServer] 1: serverSockfd: %d, , totalReceivedBytes: %d\n", serverSockfd, totalReceivedBytes);
//	cout << "[Server::receiveMsgFromServer] : serverSockfd: " << serverSockfd << ", totalReceivedBytes: " << totalReceivedBytes << ", maximalReceivedBytes: " << maximalReceivedBytes << endl;
	char * ptr = msg;

	while (totalReceivedBytes < maximalReceivedBytes) {
		printf("totalReceivedBytes: %d, maximalReceivedBytes: %d\n", totalReceivedBytes, maximalReceivedBytes);
//		cout << "totalReceivedBytes: " << totalReceivedBytes << ", maximalReceivedBytes: " << maximalReceivedBytes << endl;
		int ret = recv(serverSockfd, ptr, maximalReceivedBytes - totalReceivedBytes, 0);
		printf("ret: %d\n", ret);
//		cout << "ret: " << ret << endl;
		printf("msg: %s\n", msg);
//		cout << "msg: " << msg << endl;

		if (ret == 0) {
			printf("Error: Server is terminated. exit...\n");
//			cout << "Error: Server is terminated. exit..." << endl;
			exit(1);
		}

		if (ret < 0) {
			printf("trying one more time\n");
			// trying to receive one more time after failing the first time
			ret = recv(serverSockfd, ptr, maximalReceivedBytes - totalReceivedBytes, 0);
			printf("ret: %d\n", ret);
//			cout << "ret: " << ret << endl;

			if (ret <= 0) {
				printf("Error: Server is terminated. exit...\n");
				exit(1);
			}
		}

		totalReceivedBytes += ret;
		ptr += ret;
	}

	return totalReceivedBytes;
}


bool Client::sendMsgAndWait(char* serialized, int length, int command) {
	sendMsg(serialized, length);
	char* retVal = new char[MAX_RET_VAL_LENGTH+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_RET_VAL_STATUS];
	int retValLen = 0;
	int status = 0;

	printf("wait for receive\n");
	readCommonServerResponse(sockfd, retVal, &retValLen, &status);

	if (status == 0) {
		printf("ERROR: got failure status!\n");
		return false;
	}

	printf("got success status\n");

	if (retValLen > 0) {
		printf("received message length > 0. Calling to handleReturnValue\n");
		handleReturnValue(status, retVal+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_RET_VAL_STATUS, retValLen, command);
	}

	return true;

	/*
	//	cout << "wait for receive" << endl;
	int ret = recv(sockfd, status, 1, 0);

	if (ret < 0) {
		// trying to receive one more time after failing the first time
		ret = recv(sockfd, status, 1, 0);
	}

	if (ret < 0) {
		printf("ERROR: got failure status!\n");
//		cout << "ERROR: got failure status!" << endl;
		return false;
	}

	printf("got success status\n");
//	cout << "got success status" << endl;
	return true;
	*/
}



// todo for click usage.
CLICK_ENDDECLS
ELEMENT_PROVIDES(TCPClient)




//int main(int argc, char *argv[])
//{
//	cout << "start client" << endl;
//
//	Client *client = new Client(9095, "127.0.0.1");
//	client->connectToServer();
//	client->send(NULL, NULL);
//	return 0;
//}
