/*
 * asyncServer.cc
 *
 *  Created on: Mar 3, 2017
 *      Author: Tal
 */

#ifndef TCP_SERVERS_ASYNCSERVER_CC_
#define TCP_SERVERS_ASYNCSERVER_CC_

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


Server::Server(int serverPort) {
	yes = 1;
	port = serverPort;
	_threadIdSequencer = 0;
	listener = -1;
	efd = -1;

	cout << "is debug Mode?" << endl;
	if (DEBUG) {
		cout << "Yes" << endl;
	} else {
		cout << "No" << endl;
	}

	cout << "Server works *asynchronously*" << endl;
}


int Server::make_socket_non_blocking (int sfd) {
	int flags, s;

	flags = fcntl (sfd, F_GETFL, 0);
	if (flags == -1) {
		perror ("fcntl");
		return -1;
	}

	flags |= O_NONBLOCK;
	s = fcntl (sfd, F_SETFL, flags);
	if (s == -1) {
		perror ("fcntl");
		return -1;
	}

	return 0;
}


void Server::create_and_bind () {

	if((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		cout << "ERROR: Server-socket() error lol!" << endl;
//	  DEBUG_STDOUT(cout << "ERROR: Server-socket() error lol!" << endl);
		exit(1);
	}

  	// prevent "address already in use" error
	if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		cout << "ERROR: Server-setsockopt() error lol!" << endl;
//  		DEBUG_STDOUT(cout << "ERROR: Server-setsockopt() error lol!" << endl);
  		exit(1);
  	}

  	// bind
  	serveraddr.sin_family = AF_INET;
  	serveraddr.sin_addr.s_addr = INADDR_ANY;
  	serveraddr.sin_port = htons(port);
  	memset(&(serveraddr.sin_zero), '\0', 8);

  	if(bind(listener, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
  		cout << "ERROR: Server-bind() error lol!" << endl;
//  		DEBUG_STDOUT(cout << "ERROR: Server-bind() error lol!" << endl);
  		exit(1);
  	}

  	// listen
  	if(listen(listener, 1000) == -1) {
  		cout << "ERROR: Server-listen() error lol!" << endl;
//  		 DEBUG_STDOUT(cout << "ERROR: Server-listen() error lol!" << endl);
  		 exit(1);
  	}

  	cout << "server init completed successfully" << endl;

}


void Server::init() {
	create_and_bind();

	if (make_socket_non_blocking(listener) == -1)
		exit(1);

	if (listen (listener, SOMAXCONN) == -1) {
		cout << "ERROR: Server-listen() error lol!" << endl;
//		DEBUG_STDOUT(cout << "ERROR: Server-listen() error lol!" << endl);
		exit(1);
	}

	efd = epoll_create1(0);
	if (efd == -1) {
		cout << "ERROR: epoll_create1() error lol!" << endl;
		exit(1);
	}

	event.data.fd = listener;
	event.events = EPOLLIN;		//event.events = EPOLLIN | EPOLLET;

	if (epoll_ctl(efd, EPOLL_CTL_ADD, listener, &event) == -1) {
		cout << "ERROR: epoll_ctl() error lol!" << endl;
		exit(1);
	}

	/* Buffer where events are returned */
	events = (epoll_event *)calloc (MAXEVENTS, sizeof(struct epoll_event));

	initSpinLock(&threadsToDeleteLock);

	cout << "server init completed successfully (efd = " << efd << ")" << endl;
}

void Server::initThreadInfo (ThreadInfo* info, string hostName) {
	cout << "[EpollServer::initThreadInfo] hostName: " << hostName << endl;

	//set host name
//	memset(info->srcHostName, '\0', NI_MAXHOST);
//	memcpy(info->srcHostName, hostName, NI_MAXHOST);
	info->srcHostName = hostName;

	// set thread id
	info->id = ++_threadIdSequencer;

	// init sock fds set
	info->sockfds = new set<int>;

	// create new efd
	info->efd = epoll_create1(0);

	if (info->efd == -1) {
		cout << "ERROR: epoll_create1() error lol!" << endl;
		exit(1);
	}

	/* Buffer where events are returned */
	info->events = (epoll_event *)calloc (MAXEVENTS, sizeof(struct epoll_event));

	// init spin lock
	initSpinLock(&(info->lock));

	// add to new thread to threadsInfo map
	threadsInfo.insert(make_pair(info->srcHostName, info));

	printf("[EpollServer::initThreadInfo] End\n");

}



void Server::addNewClient() {

	int s;

	while (1) {
		struct sockaddr in_addr;
		socklen_t in_len;
		int infd;
		char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
//		char* hbuf = new char[NI_MAXHOST];

		in_len = sizeof in_addr;
		infd = accept (listener, &in_addr, &in_len);
		if (infd == -1) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				/* We have processed all incoming
				   connections. */
				break;
			} else {
				perror ("accept");
				break;
			}
		}

		s = getnameinfo (&in_addr, in_len, hbuf, NI_MAXHOST*sizeof(char), sbuf, sizeof sbuf,
				NI_NUMERICHOST | NI_NUMERICSERV);

		if (s == 0) {
			printf("Accepted connection on descriptor %d "
					"(host=%s, port=%s)\n", infd, hbuf, sbuf);
		}

		/* Make the incoming socket non-blocking and add it to the
		   list of fds to monitor. */

		printf("threadsInfo.size = %d. state before:\n", threadsInfo.size());
		for (map<string, ThreadInfo*>::iterator iter = threadsInfo.begin(); iter != threadsInfo.end(); iter++) {
			ThreadInfo* info = iter->second;
			cout << "" << iter->first << " --> [" << info->srcHostName << ", id: " << info->id << ", efd: " << info->efd << "]" << endl;
		}

		bool shouldCreateNewThread = false;
		ThreadInfo* threadInfo = getOrCreateThreadInfo(string(hbuf), &shouldCreateNewThread);
		addSockfdToThreadInfo(threadInfo, infd);

		if (shouldCreateNewThread) {
			cout << "Creating a new thread for src hostname: " << threadInfo->srcHostName << ", id: " << threadInfo->id << endl;
			ThreadArgs* threadArgs = new ThreadArgs;
			threadArgs->server = this;
			threadArgs->info = threadInfo;
			pthread_t* thread = new pthread_t;
			activeThreads.insert(make_pair(threadInfo->id, thread));
			cout << "current thread id: " << pthread_self() << endl;
			pthread_create(thread, NULL, &Server::threadRunnerHelper, (void*)threadArgs);
//			pthread_detach(*thread);
		}

		printf("threadsInfo.size = %d. state after:\n", threadsInfo.size());
		for (map<string, ThreadInfo*>::iterator iter = threadsInfo.begin(); iter != threadsInfo.end(); iter++) {
			ThreadInfo* info = iter->second;
			cout << "" << iter->first << " --> [" << info->srcHostName << ", id: " << info->id << ", efd: " << info->efd << "]" << endl;
		}
	}
}

ThreadInfo* Server::getOrCreateThreadInfo (string hostName, bool* shouldCreateNewThread) {
	cout << "[EpollServer::getOrCreateThreadInfo] hostName: " << hostName << endl;


	if (threadsInfo.find(hostName) != threadsInfo.end()) {
//		cout << "threadsInfo has " << hostName << endl;
		*shouldCreateNewThread = false;
		return threadsInfo[hostName];
	} else {
//		cout << "threadsInfo doesn't has " << hostName << ". Creating new one.." << endl;
		ThreadInfo *info = new ThreadInfo;
		initThreadInfo(info, hostName);
		*shouldCreateNewThread = true;
		return info;
	}

	printf("[EpollServer::getOrCreateThreadInfo] End\n");
}

void Server::addSockfdToThreadInfo(ThreadInfo* info, int infd) {

	printf("[EpollServer::addSockfdToThreadInfo] infd: %d\n", infd);

	/* Make the incoming socket non-blocking and add it to the
	   list of fds to monitor. */
	int s = make_socket_non_blocking (infd);
	if (s == -1)
		abort ();

//	printf("1\n");

	lockSpinLock(&(info->lock));

	info->event.data.fd = infd;
	info->event.events = EPOLLIN;		//EPOLLIN | EPOLLET;
	s = epoll_ctl (info->efd, EPOLL_CTL_ADD, infd, &(info->event));

//	printf("4\n");

	if (s == -1) {
		perror ("epoll_ctl");
		abort ();
	}

//	printf("5\n");

	unlockSpinLock(&(info->lock));

	info->sockfds->insert(infd);

	printf("num of sockets to listen to: %d (", (int)info->sockfds->size());
	for (set<int>::iterator iter = info->sockfds->begin(); iter != info->sockfds->end(); iter++) {
		printf("%d, ", *iter);
	}
	printf(")\n");

	printf("[EpollServer::addSockfdToThreadInfo] End\n");
}

void Server::removeClient(int sockfdToRemove, int numOfReceivedBytes) {

	cout << "Start removing client " << sockfdToRemove << endl;

	// got error or connection closed by client
	if(numOfReceivedBytes == 0) {
		// connection closed
		cout << "socket " << sockfdToRemove << " hung up" << endl;
	} else {
		cout << "ERROR: recv() error lol!" << endl;
	}

	doRemoveClient(sockfdToRemove, NULL);

	cout << "Done removing client " << sockfdToRemove << endl;
}

void Server::doRemoveClient(int sockfdToRemove, ThreadInfo* info) {
	cout << "[EpollServer::doRemoveClient] Start" << endl;

	if (info == NULL) {
		cout << "info is NULL !!" << endl;
		info = getThreadInfoBySockfd(sockfdToRemove);

		if (info == NULL) {
			return;
		}
	}

	lockSpinLock(&(info->lock));
	if (info->sockfds->find(sockfdToRemove) != info->sockfds->end()) {
		info->sockfds->erase(sockfdToRemove);
	}
	unlockSpinLock(&(info->lock));

	/* Closing the descriptor will make epoll remove it
	   from the set of descriptors which are monitored. */
	close(sockfdToRemove);
	removeOrphanThreads();
	cout << "[EpollServer::doRemoveClient] Done";
}

ThreadInfo* Server::getThreadInfoBySockfd(int sockfd) {
	cout << "[EpollServer::getThreadInfoBySockfd] Start" << endl;
	cout << "sockfd: " << sockfd << endl;
	ThreadInfo* info = NULL;

	for (map<string, ThreadInfo*>::iterator iter = threadsInfo.begin(); iter != threadsInfo.end(); iter++) {
		info = iter->second;

		if (info->sockfds->find(sockfd) != info->sockfds->end()) {
			break;
		}
	}

	cout << "[EpollServer::getThreadInfoBySockfd] End" << endl;
	return info;
}



bool Server::run() {
	int s;

	/* The event loop */
	while (1) {
		int n, i;

		n = epoll_wait (efd, events, MAXEVENTS, -1);

		cout << "Server-epoll_wait() is OK..." << endl;
		cout << "[Server::run] current thread id: " << pthread_self() << endl;
//		DEBUG_STDOUT(cout << "Server-epoll_wait() is OK..." << endl);

		bool a = false;
		a = true;

		deleteThreadsFromSet();

		for (i = 0; i < n; i++) {
			if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) ||
					(!(events[i].events & EPOLLIN))) {
				/*  An error has occured on this fd, or the socket is not
					ready for reading (why were we notified then?) */
				cout << "ERROR: epoll error. continue to next socket.." << endl;
				doRemoveClient(events[i].data.fd, NULL);
//				close(events[i].data.fd);
				continue;

			} else if (listener == events[i].data.fd) {
				/* We have a notification on the listening socket, which
	               means one or more incoming connections. */
				addNewClient();
				continue;
			} /*else {
				// We have data on the fd waiting to be read. Read and
	            //   display it. We must read whatever data is available
	            //   completely, as we are running in edge-triggered mode
	            //   and won't get a notification again for the same
	            //   data.

				handleClientRequest(events[i].data.fd);
			}*/
		}
	}

	free (events);
	close(listener);

	return true;
}

void Server::runThread(ThreadInfo* info) {
	int s;
	long tid = pthread_self();
	cout << "[Server::runThread] Start (tid: " << tid << ")" << endl;


	/* The event loop */
	while (1) {
		int n, i;

		n = epoll_wait (info->efd, info->events, MAXEVENTS, -1);
//		cout << "[runThread: " << tid << "] Server-epoll_wait() is OK for " << info->srcHostName << "." << endl;
//		DEBUG_STDOUT(cout << "Server-epoll_wait() is OK for " << info->srcHostName << "..." << endl);

		for (i = 0; i < n; i++) {
//			cout << "1" << endl;
			if ((info->events[i].events & EPOLLERR) || (info->events[i].events & EPOLLHUP) ||
					(!(info->events[i].events & EPOLLIN))) {
				/*  An error has occured on this fd, or the socket is not
					ready for reading (why were we notified then?) */
				cout << "ERROR: epoll error. continue to next socket.." << endl;
				doRemoveClient(events[i].data.fd, NULL);	//doRemoveClient(events[i].data.fd, info);
//				close(info->events[i].data.fd);
				continue;

			} else {
				/* We have data on the fd waiting to be read. Read and
	               display it. We must read whatever data is available
	               completely, as we are running in edge-triggered mode
	               and won't get a notification again for the same
	               data. */

				handleClientRequest(info->events[i].data.fd);
			}
		}
	}
}


void Server::removeOrphanThreads() {
	cout << "[EpollServer::removeOrphanThreads] Start" << endl;

	for (map<string, ThreadInfo*>::iterator iter = threadsInfo.begin(); iter != threadsInfo.end(); ) {
		ThreadInfo* info = iter->second;
		lockSpinLock(&(info->lock));

		if (info->sockfds->size() == 0) {
			cout << "erasing thread id: " << info->id << ", hostname: " << info->srcHostName << endl;
			delete info->events;
			delete info;

			addThreadToDeleteSet(activeThreads[info->id]);
			activeThreads.erase(info->id);
			threadsInfo.erase(iter++);
		} else {
			cout << "thread id: " << info->id << ", hostname: " << info->srcHostName << ", sockfdsSize: " << info->sockfds->size() << endl;
			iter++;
		}

		unlockSpinLock(&(info->lock));
	}

	cout << "[EpollServer::removeOrphanThreads] End" << endl;
}

void Server::addThreadToDeleteSet(pthread_t* th) {
	cout << "[EpollServer::addThreadToDeleteSet] Start" << endl;

	lockSpinLock(&threadsToDeleteLock);
	threadsToDelete.insert(th);
	unlockSpinLock(&threadsToDeleteLock);

	cout << "[EpollServer::addThreadToDeleteSet] End" << endl;
}


void Server::deleteThreadsFromSet() {
	cout << "[EpollServer::deleteThreadsFromSet] Start" << endl;
	lockSpinLock(&threadsToDeleteLock);

	for (set<pthread_t*>::iterator iter = threadsToDelete.begin(); iter != threadsToDelete.end(); iter++) {
		pthread_t* th = *iter;
		cout << "~" << endl;
		delete th;
	}

	threadsToDelete.clear();

	unlockSpinLock(&threadsToDeleteLock);
	cout << "[EpollServer::deleteThreadsFromSet] End" << endl;
}




////////////////////////////////////////////////////////////////////////////////////////////



void Server::handleClientRequest(int sockfd) {
//	cout << "about to read message from sockfd: " << sockfd << endl;

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

//	for (int i=0; i<msgLen; i++) {
//		printf("%c", msg[i]);
//	}
//	cout << endl;

	retVal = new char[MAX_RET_VAL_LENGTH+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_RET_VAL_STATUS];

//	cout << "0" << endl;
//
//	cout << "command is: " << command << endl;
//	cout << "msg is: " << msg << endl;
//	cout << "msgLen is: " << msgLen << endl;

	// now we have msg and msgLen.
	void* obj = deserializeClientRequest(command, msg+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_COMMAND_PREFIX, msgLen);
//	cout << "obj" << endl;
	bool retStatus = processRequest(obj, command, retVal+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_RET_VAL_STATUS, &retValLen);
//	cout << "1" << endl;

	writeResponseToClient(sockfd, retStatus, retVal, retValLen);
//	cout << "2" << endl;

	delete msg;
//	cout << "3" << endl;
	delete retVal;
//	cout << "4" << endl;
	freeDeserializedObject(obj, command);
//	cout << "5" << endl;
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

//	cout << "in sendMsg.. length: " << length << ", msg:";

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

//		int ret = send(sockfd, retVal, length - totalSentBytes, 0);
		int ret = write(sockfd, retVal, length - totalSentBytes);

		if (ret == 0) {
			cout << "ERROR: The client is terminated. Stop sending.." << endl;
			break;
		}

		if (ret < 0) {
			cout << "ERROR: Failed to send to socket (" << sockfd << "). Trying one more time.." << endl;
			cout << "Reason: " << errno << endl;

			int tries = 20;
			while (ret < 0 && errno == EAGAIN && tries > 0) {
				cout << "Waiting 50 msec before trying to send again" << endl;
				usleep(50*1000);
				cout << "Trying again.." << endl;

				for(int i=0; i< length; i++) {
					printf("%c", retVal[i]);
				}
				cout << endl;

				ret = write(sockfd, retVal, length - totalSentBytes);

				if (ret == 0) {
					cout << "ERROR: The client is terminated. Stop sending.." << endl;
					return false;
				}

				tries--;
			}

			if (ret < 0 && errno != EAGAIN) {
				cout << "~ERROR: Failed to send to socket (" << sockfd << "). Trying one more time.." << endl;
				cout << "Reason: " << errno << endl;
				return false;
			}

			/*
			if (errno == EAGAIN) {
				cout << "Waiting 1 msec before trying to send again" << endl;
				usleep(1000);
				cout << "Trying again.." << endl;
			}


			// trying to send one more time after failing the first time
//			ret = send(sockfd, retVal, length - totalSentBytes, 0);
			ret = write(sockfd, retVal, length - totalSentBytes);

			if (ret == 0) {
				cout << "ERROR: The client is terminated. Stop sending.." << endl;
				exit(1);
			}

			if (ret < 0) {
				return false;
			}
			*/
		}

		totalSentBytes += ret;
		retVal += ret;
	}

	DEBUG_STDOUT(cout << "totalSentBytes: " << totalSentBytes << endl);
//	cout << "totalSentBytes: " << totalSentBytes << endl;

	if (totalSentBytes == length) {
		return true;
	}

	return false;

}

void Server::writeResponseToClient(int sockfd, bool succeed, char* retVal, int retValLen) {
//	cout << "[Server::writeResponseToClient] Start" << endl;
	int ret = -1;
	char numAsStr[NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+1];
	intToStringDigits(retValLen, NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX, numAsStr);

	DEBUG_STDOUT(cout << "numAsStr is: " << numAsStr << ", retValLen is: " << retValLen << endl);
//	cout << "numAsStr is: " << numAsStr << ", retValLen is: " << retValLen << endl;

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
//	cout << "sending " << retValLen << " bytes as response to client with sockfd: " << sockfd << endl;

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

//	cout << "Server::readCommonClientRequest] 1: sockfd: " << sockfd << ", totalReceivedBytes: " << totalReceivedBytes << endl;

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
//	cout << "msgLen is: " << *msgLen << ", len: " << len << endl;

	// convert the received command digit to int
	char receivedCommand[NUM_OF_DIGITS_FOR_COMMAND_PREFIX + 1];
	memset(receivedCommand, '\0', NUM_OF_DIGITS_FOR_COMMAND_PREFIX + 1);
	strncpy(receivedCommand, msg+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX, NUM_OF_DIGITS_FOR_COMMAND_PREFIX);
	*command = checkLength(receivedCommand, NUM_OF_DIGITS_FOR_COMMAND_PREFIX, 0);
	DEBUG_STDOUT(cout << "command is: " << *command << ", receivedCommand: " << receivedCommand << endl);
//	cout << "command is: " << *command << ", receivedCommand: " << receivedCommand << endl;

	// receive the content
	totalReceivedBytes = receiveMsgFromClient(sockfd, totalReceivedBytes, ptr, *msgLen+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_COMMAND_PREFIX);
	DEBUG_STDOUT(cout << "Server::readCommonClientRequest] 2: totalReceivedBytes: " << totalReceivedBytes << endl);
//	cout << "Server::readCommonClientRequest] 2: totalReceivedBytes: " << totalReceivedBytes << endl;

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

		// async version !!	todo !!
		ssize_t ret = read (clientSockfd, ptr, maximalReceivedBytes - totalReceivedBytes);
		DEBUG_STDOUT(cout << "ret: " << ret << endl);
		if (ret == 0) {
			// End of file. The remote has closed the connection.
			removeClient(clientSockfd, ret);
			return -1;
		}

		if (ret == -1) {
			// If errno == EAGAIN, that means we have read all data. So go back to the main loop.
			if (errno != EAGAIN) {
				DEBUG_STDOUT(cout << "trying one more time" << endl);
				ret = read (clientSockfd, ptr, maximalReceivedBytes - totalReceivedBytes);
				DEBUG_STDOUT(cout << "ret: " << ret << endl);

				if (ret <= 0) {
					removeClient(clientSockfd, ret);
					return -1;
				}
			}

			break;

		}
		// end async version !!


		/*
		// sync version
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
		*/

		totalReceivedBytes += ret;
		ptr += ret;
	}

	return totalReceivedBytes;
}

void* Server::deserializeClientRequest(int command, char* msg, int msgLen) {
	DEBUG_STDOUT(cout << "Server::deserializeClientRequest" << endl);
	cout << "Server::deserializeClientRequest" << endl;
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




#endif /* TCP_SERVERS_ASYNCSERVER_CC_ */
