/*
 * based on: http://www.tenouk.com/Module41.html
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "server.hh"



Server::Server(int port) {
	fdmax = -1;
	listener = -1;
	newfd = -1;
	yes = 1;
	Server::port = port;

	cout << "is debug Mode?" << endl;
	if (DEBUG) {
		cout << "Yes" << endl;
	} else {
		cout << "No" << endl;
	}
}


void Server::init() {
	// clear the master and temp sets
	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	if((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		DEBUG_STDOUT(cout << "ERROR: Server-socket() error lol!" << endl);
		exit(1);
	}

	// prevent "address already in use" error
	if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		DEBUG_STDOUT(cout << "ERROR: Server-setsockopt() error lol!" << endl);
		exit(1);
	}

	// bind
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = INADDR_ANY;
	serveraddr.sin_port = htons(port);
	memset(&(serveraddr.sin_zero), '\0', 8);

	if(bind(listener, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
		DEBUG_STDOUT(cout << "ERROR: Server-bind() error lol!" << endl);
		exit(1);
	}

	// listen
	if(listen(listener, 1000) == -1) {
		 DEBUG_STDOUT(cout << "ERROR: Server-listen() error lol!" << endl);
		 exit(1);
	}

	// add the listener to the master set
	FD_SET(listener, &master);

	// keep track of the biggest file descriptor
	fdmax = listener;
	cout << "server init completed successfully" << endl;

}

bool Server::run() {
	while(1) {
		pthread_mutex_lock(&master_set_mtx);
		read_fds = master;
		pthread_mutex_unlock(&master_set_mtx);

		if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			DEBUG_STDOUT(cout << "ERROR: Server-select() error lol! " << endl);
			DEBUG_STDOUT(cout << "Reason: " << errno << endl);
			exit(1);
		}

		DEBUG_STDOUT(cout << "Server-select() is OK..." << endl);

		if(FD_ISSET(listener, &read_fds)) {
			addNewClient();	// handle new connections
		} else {
			//run through the existing connections looking for data to be read
			for (set<int>::iterator it=connectedClients.begin(); it!=connectedClients.end();) {
				set<int>::iterator currIter = it++;
				DEBUG_STDOUT(cout << "currIter: " << *currIter << endl);

				if(FD_ISSET(*currIter, &read_fds)) {
					DEBUG_STDOUT(cout << "start handling new request from sockfd: " << *currIter << endl);

					// sync handling
					handleClientRequest(*currIter);
					break;	// todo It makes me give higher priority to the first registered clients.
				}
			}
		}
	}
}

void Server::addNewClient() {
	cout << "Adding new client" << endl;

	socklen_t addrlen = sizeof(clientaddr);
	if((newfd = accept(listener, (struct sockaddr *)&clientaddr, &addrlen)) == -1) {
		DEBUG_STDOUT(cout << "ERROR: Server-accept() error lol!" << endl);
	} else {
		DEBUG_STDOUT(cout << "~ Server-accept() is OK..." << endl);

		pthread_mutex_lock(&master_set_mtx);

		connectedClients.insert(newfd);
		FD_SET(newfd, &master); // add to master set

		if(newfd > fdmax) {
			fdmax = newfd;
		}

		pthread_mutex_unlock(&master_set_mtx);

		cout << "New connection from " << inet_ntoa(clientaddr.sin_addr) << " on socket " << newfd << endl;
	}
}

void Server::removeClient(int sockfdToRemove, int numOfReceivedBytes) {
	pthread_mutex_lock(&master_set_mtx);

	cout << "Start removing client " << sockfdToRemove << endl;

	if (!FD_ISSET(sockfdToRemove, &master)) {
		DEBUG_STDOUT(cout << "sockfdToRemove: " << sockfdToRemove << " is already removed" << endl);
		pthread_mutex_unlock(&master_set_mtx);
		return;
	}

	// got error or connection closed by client
	if(numOfReceivedBytes == 0) {
		// connection closed
		cout << "socket " << sockfdToRemove << " hung up" << endl;
	} else {
		cout << "ERROR: recv() error lol!" << endl;
	}

	// remove from master set
	FD_CLR(sockfdToRemove, &master);
	FD_CLR(sockfdToRemove, &read_fds);
	connectedClients.erase(sockfdToRemove);

	// close it...
	close(sockfdToRemove);

	cout << "Done removing client " << sockfdToRemove << endl;

	pthread_mutex_unlock(&master_set_mtx);
}

void Server::handleClientRequest(int sockfd) {
	DEBUG_STDOUT(cout << "about to read message from sockfd: " << sockfd << endl);
	char* msg = new char[SERVER_BUFFER_SIZE+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_RET_VAL_STATUS];
	int msgLen;	// msgLen doesn't include the command chars (as well as the msgLen digits themselves)
	int command;
	char* retVal = NULL;
	int retValLen = 0;

	readCommonClientRequest(sockfd, msg, &msgLen, &command);
	DEBUG_STDOUT(cout << "[Server::handleClientRequest] msgLen: " << msgLen << ", command: " << command << endl);

	if (msgLen == -1) {
		delete msg;
		DEBUG_STDOUT(cout << "[Server::handleClientRequest] stop reading from sockfd " << sockfd << endl);
		return;
	}


	// for debug usage
	if (DEBUG)
		printMsg(msg, msgLen);

	retVal = new char[MAX_RET_VAL_LENGTH+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_RET_VAL_STATUS];

	// now we have msg and msgLen.
	void* obj = deserializeClientRequest(command, msg+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_COMMAND_PREFIX, msgLen);
	bool retStatus = processRequest(obj, command, retVal+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_RET_VAL_STATUS, &retValLen);

	writeResponseToClient(sockfd, retStatus, retVal, retValLen);

	delete msg;
	delete retVal;
	freeDeserializedObject(obj, command);

	// relevant when using multi-threading
	FD_SET(newfd, &master); // add back to master set
	FD_SET(newfd, &read_fds); // add back to read_fds set

}

/*
 * This function returns a string representation of length numOfDigits for the inserted number. If the number's
 * length is smaller than numOfDigits, we pad the returned string with 0 at the beginning appropriately.
 */
void Server::intToStringDigits (int number, uint8_t numOfDigits, char* numAsStr)
{
	sprintf(numAsStr, "%0*d", numOfDigits, number);
}

bool Server::sendMsg(int sockfd, char* retVal, int length) {
	DEBUG_STDOUT(cout << "in sendMsg.. length: " << length << ", msg:");

	if (DEBUG) {
		// print message content
		for(int i=0; i< length; i++) {
			printf("%c", retVal[i]);
		}
		cout << endl;
	}

	int totalSentBytes = 0;

	if (length > MAX_RET_VAL_LENGTH+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_RET_VAL_STATUS) {
		cout << "ERROR: can't send message (" << length << ") that is too long (" << (MAX_RET_VAL_LENGTH+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_RET_VAL_STATUS) << ")" << endl;
		return false;
	}

	// Write the message to the server
	while (totalSentBytes < length) {
		DEBUG_STDOUT(cout << "sending.. totalSentBytes: " << totalSentBytes << endl);

		int ret = send(sockfd, retVal, length - totalSentBytes, 0);

		if (ret == 0) {
			cout << "ERROR: The client is terminated. Stop sending.." << endl;
			break;
		}

		if (ret < 0) {
			cout << "ERROR: Failed to send. Trying one more time.." << endl;
			cout << "Reason: " << errno << endl;

			// trying to send one more time after failing the first time
			ret = send(sockfd, retVal, length - totalSentBytes, 0);

			if (ret == 0) {
				cout << "ERROR: The client is terminated. Stop sending.." << endl;
				exit(1);
			}

			if (ret < 0) {
				return false;
			}
		}

		totalSentBytes += ret;
		retVal += ret;
	}

	DEBUG_STDOUT(cout << "totalSentBytes: " << totalSentBytes << endl);

	if (totalSentBytes == length) {
		return true;
	}

	return false;

}

void Server::writeResponseToClient(int sockfd, bool succeed, char* retVal, int retValLen) {
	int ret = -1;
	char numAsStr[NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+1];
	intToStringDigits(retValLen, NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX, numAsStr);

	DEBUG_STDOUT(cout << "numAsStr is: " << numAsStr << ", retValLen is: " << retValLen << endl);

	for (int i=0; i<NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX; i++) {
		retVal[i] = numAsStr[i];
	}

	if (DEBUG) {
		for (int i=0; i<NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX; i++) {
			printf("numAsStr[%d]: %c\n", i, numAsStr[i]);
		}
	}

	if (succeed) {
		retVal[NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX] = '1';
	} else {
		retVal[NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX] = '0';
	}

	// retValLen is the length of the returned data
	// without NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_RET_VAL_STATUS digits.
	// Therefore we need to add them to the total length that should be sent back to client.
	retValLen += NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX + NUM_OF_DIGITS_FOR_RET_VAL_STATUS;
	DEBUG_STDOUT(cout << "sending " << retValLen << " bytes as response to client with sockfd: " << sockfd << endl);

	sendMsg(sockfd, retVal, retValLen);
}

void Server::printMsg(char* msg, int msgLen) {
	cout << "message full len is: " << msgLen + NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX + NUM_OF_DIGITS_FOR_COMMAND_PREFIX << endl;
	cout << "message len is: " << msgLen << endl;
}


void Server::readCommonClientRequest(int sockfd, char* msg, int* msgLen, int* command) {

	// receive message in the following format: [7 digits representing the client name's length][client name]
	int totalReceivedBytes = receiveMsgFromClient(sockfd, 0, msg, NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_COMMAND_PREFIX);
	DEBUG_STDOUT(cout << "Server::readCommonClientRequest] 1: sockfd: " << sockfd << ", totalReceivedBytes: " << totalReceivedBytes << endl);

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
	*msgLen = checkLength(len, NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX, 1);
	DEBUG_STDOUT(cout << "msgLen is: " << *msgLen << ", len: " << len << endl);

	// convert the received command digit to int
	char receivedCommand[NUM_OF_DIGITS_FOR_COMMAND_PREFIX + 1];
	memset(receivedCommand, '\0', NUM_OF_DIGITS_FOR_COMMAND_PREFIX + 1);
	strncpy(receivedCommand, msg+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX, NUM_OF_DIGITS_FOR_COMMAND_PREFIX);
	*command = checkLength(receivedCommand, NUM_OF_DIGITS_FOR_COMMAND_PREFIX, 0);
	DEBUG_STDOUT(cout << "command is: " << *command << ", receivedCommand: " << receivedCommand << endl);

	// receive the content
	totalReceivedBytes = receiveMsgFromClient(sockfd, totalReceivedBytes, ptr, *msgLen+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_COMMAND_PREFIX);
	DEBUG_STDOUT(cout << "Server::readCommonClientRequest] 2: totalReceivedBytes: " << totalReceivedBytes << endl);

	// client socket was closed and removed
	if (totalReceivedBytes == -1) {
		return;
	}

}

/*
 * This function checks that the inserted num is actually a number, and that is at least of minimalExpectedValue.
 * length is num size. If num passed these checks - returns it as integer. Otherwise returns -1.
 */
int Server::checkLength (char* num, int length, int minimalExpectedValue) {
	for (int i=0; i<length; i++) {
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
int Server::receiveMsgFromClient (int clientSockfd, int totalReceivedBytes, char* msg, int maximalReceivedBytes)
{
	DEBUG_STDOUT(cout << "[Server::receiveMsgFromClient] : clientSockfd: " << clientSockfd << ", totalReceivedBytes: " << totalReceivedBytes << ", maximalReceivedBytes: " << maximalReceivedBytes << endl);
	char * ptr = msg;

	while (totalReceivedBytes < maximalReceivedBytes) {
		DEBUG_STDOUT(cout << "totalReceivedBytes: " << totalReceivedBytes << ", maximalReceivedBytes: " << maximalReceivedBytes << endl);
		int ret = recv(clientSockfd, ptr, maximalReceivedBytes - totalReceivedBytes, 0);
		DEBUG_STDOUT(cout << "ret: " << ret << endl);

		if (ret == 0) {
			removeClient(clientSockfd, ret);
			return -1;
		}

		if (ret < 0) {
			DEBUG_STDOUT(cout << "trying one more time" << endl);

			// trying to receive one more time after failing the first time
			ret = recv(clientSockfd, ptr, maximalReceivedBytes - totalReceivedBytes, 0);
			DEBUG_STDOUT(cout << "ret: " << ret << endl);

			if (ret <= 0) {
				removeClient(clientSockfd, ret);
				return -1;
			}
		}

		totalReceivedBytes += ret;
		ptr += ret;
	}

	return totalReceivedBytes;
}

void* Server::deserializeClientRequest(int command, char* msg, int msgLen) {
	DEBUG_STDOUT(cout << "Server::deserializeClientRequest" << endl);
	return NULL;
}

bool Server::processRequest(void* obj, int command, char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "Server::processRequest" << endl);
	return true;
}

void Server::freeDeserializedObject(void* obj, int command) {
	DEBUG_STDOUT(cout << "Server::freeObject" << endl);
}

void Server::initSpinLock(pthread_spinlock_t* lock) {
//	cout << "[Server::initSpinLock] Start" << endl;

	int ret = pthread_spin_init(lock, PTHREAD_PROCESS_SHARED);

	if (ret != 0) {
		perror ("pthread_spin_init error");
		abort ();
	}

//	cout << "spin lock was initialized" << endl;
}


void Server::lockSpinLock(pthread_spinlock_t* lock) {
//	cout << "trying to pass spin lock" << endl;
	int ret = pthread_spin_lock(lock);

	if (ret != 0) {
		perror ("pthread_spin_lock error");
		abort ();
	}

//	cout << "spin lock is locked" << endl;
}

void Server::unlockSpinLock(pthread_spinlock_t* lock) {
	int ret = pthread_spin_unlock(lock);

	if (ret != 0) {
		perror ("pthread_spin_lock error");
		abort ();
	}

//	cout << "spin lock is free" << endl;
}

void Server::initMutex(pthread_mutex_t* lock) {
//	cout << "[Server::initMutex] Start" << endl;

	int ret = pthread_mutex_init(lock, NULL);

	if (ret != 0) {
		perror ("pthread_mutex_init error");
		abort ();
	}

//	cout << "mutex was initialized" << endl;
}


void Server::lockMutex(pthread_mutex_t* lock) {
//	cout << "trying to pass mutex" << endl;
	int ret = pthread_mutex_lock(lock);

	if (ret != 0) {
		perror ("pthread_mutex_lock error");
		abort ();
	}

//	cout << "mutex is locked" << endl;
}


void Server::unlockMutex(pthread_mutex_t* lock) {
	int ret = pthread_mutex_unlock(lock);

	if (ret != 0) {
		perror ("pthread_mutex_unlock error");
		abort ();
	}

//	cout << "mutex is free" << endl;
}
