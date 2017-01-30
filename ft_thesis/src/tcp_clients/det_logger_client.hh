/*
 * det_logger_client.hh
 *
 *  Created on: Jan 24, 2017
 *      Author: Tal
 */

#ifndef TCP_CLIENTS_DET_LOGGER_CLIENT_HH_
#define TCP_CLIENTS_DET_LOGGER_CLIENT_HH_


#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>
#include "../common/pal_api/pals_manager.hh"
#include "client.hh"
#include <map>
#include <vector>

#define STORE_COMMAND_TYPE 0
#define GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE 1
#define GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE 2
#define DELETE_PACKETS_COMMAND_TYPE 3

using namespace std;

class DetLoggerClient : public Client {

private:
	void serializePalsManagerObject(int command, void* obj, char* serialized, int* len);
	void serializeGetPalsByMBIdAndPackId(int command, void* obj, char* serialized, int* len);
	void serializeDeleteFirstPackets(int command, void* obj, char* serialized, int* len);
	void handleGetPacketIdsResponse(int command, char* retVal, int len, void* retValAsObj);
	void handleGetPalsByMBIdAndPackIdResponse(int command, char* retVal, int retValLen, void* retValAsObj);

	// mocked client for debug
	static PALSManager* prepareDetLoggerTest1();
	static PALSManager* prepareDetLoggerTest2();
	static PALSManager* prepareDetLoggerTest3();
	static PALSManager* prepareDetLoggerTest4();
	static void* prepareGetPalsTest(uint16_t mbId, uint64_t packId);
	static void runTestAndCompare(DetLoggerClient *client);
	static void runDetLoggerTest(DetLoggerClient *client, PALSManager* pm);
	static void getProcessedPacketIds(DetLoggerClient *client, uint16_t* mbId);
	static void getPals(DetLoggerClient *client, void* msgToSend);
	// end mocked client

protected:
	void serializeObject(int command, void* obj, char* serialized, int* len);
	void handleReturnValue(int status, char* retVal, int len, int command, void* retValAsObj);

public:
	DetLoggerClient(int port, char* address) : Client(port, address) {
		// do nothing
	}

	static void runTests(char* address);
 };



#endif /* TCP_CLIENTS_DET_LOGGER_CLIENT_HH_ */
