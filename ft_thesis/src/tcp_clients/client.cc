/*
 * client.cpp
 *
 *  Created on: Dec 28, 2016
 *      Author: Tal
 */

#include "client.hh"

#include <sstream>


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
		cout << "ERROR: Could not create socket" << endl;
	}

	cout << "Socket created" << endl;

	// activate keep-alive mechanism
	int val = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof val);

	//Connect to remote server
	if (connect(sockfd , (struct sockaddr *)&sock_addr_server , sizeof(sock_addr_server)) < 0) {
		cout << "ERROR: Connect failed." << endl;
		return;
	}

	cout<<"Connected\n";
}

void Client::serializeObject(void* obj, char* serialized, int* len) {
	cout << "Client::serializeObject" << endl;
}

/*
 * This function returns a string representation of length numOfDigits for the inserted number. If the number's
 * length is smaller than numOfDigits, we pad the returned string with 0 at the beginning appropriately.
 */
string intToStringDigits (int number, uint8_t numOfDigits)
{
	string numAsString;
	stringstream ss;
	ss << number;
	numAsString = ss.str();
	while (numAsString.size() != numOfDigits)
	{
		numAsString = "0" + numAsString;
	}

	return numAsString;
}

void Client::prepareToSend(void* obj, char* serialized, int* len) {
	// we leave NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX (7) digits for the serialization length
	serializeObject(obj, serialized + NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX, len);

	string lenPrefix = intToStringDigits(*len, NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX);

	cout << "lenPrefix is: " << lenPrefix << endl;

	for (int i=0; i<NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX; i++) {
		serialized[i] = lenPrefix[i];
	}

	*len += NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX;

}

bool Client::sendMsg(char* serialized, int length) {
	cout << "in sendMsg.. length: " << length << ", msg: ";
	// print message content
	for(int i=0; i< length; i++) {
		cout << serialized[i];
	}
	cout << endl;


	int totalSentBytes = 0;

	if (length > SERVER_BUFFER_SIZE) {
		cout << "ERROR: can't send message that is longer than " << SERVER_BUFFER_SIZE << endl;
		return false;
	}

	// Write the message to the server
	while (totalSentBytes < length) {
		int ret = send(sockfd, serialized, length - totalSentBytes, 0);

		if (ret == 0) {
			cout << "ERROR: The server is terminated. exit..";
			exit(1);
		}

		if (ret < 0) {
			// trying to send one more time after failing the first time
			ret = send(sockfd, serialized, length - totalSentBytes, 0);

			if (ret == 0) {
				cout << "ERROR: The server is terminated. exit..";
				exit(1);
			}

			if (ret < 0) {
				return false;
			}
		}

		totalSentBytes += ret;
		serialized += ret;
	}

	cout << "totalSentBytes: " << totalSentBytes << endl;

	return true;

}

bool Client::sendMsgAndWait(char* serialized, int length) {
	sendMsg(serialized, length);
	char status[1];

	cout << "wait for receive" << endl;
	int ret = recv(sockfd, status, 1, 0);

	if (ret < 0) {
		// trying to receive one more time after failing the first time
		ret = recv(sockfd, status, 1, 0);
	}

	if (ret < 0) {
		cout << "ERROR: got failure status!" << endl;
		return false;
	}

	cout << "got success status" << endl;
	return true;
}

//int main(int argc, char *argv[])
//{
//	cout << "start client" << endl;
//
//	Client *client = new Client(9095, "127.0.0.1");
//	client->connectToServer();
//	client->send(NULL, NULL);
//	return 0;
//}
