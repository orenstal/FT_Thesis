/*
 * packets_logger_client.hh
 *
 *  Created on: Jan 24, 2017
 *      Author: Tal
 */

#ifndef TCP_CLIENTS_PACKETS_LOGGER_CLIENT_HH_
#define TCP_CLIENTS_PACKETS_LOGGER_CLIENT_HH_

#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>
#include "client.hh"
#include "../common/wrappedPacketData/wrapped_packet_data.hh"
#include "../common/replayPackets/replay_packets.hh"

#define DEBUG_LISTEN_TO_PORT 5556
#define DEBUG_LISTEN_TO_ADDRESS "127.0.0.1"
#define DEBUG_LISTEN_TO_ADDRESS_LEN 9

#define STORE_COMMAND_TYPE 0
#define GET_PACKET_BY_PACKID_COMMAND_TYPE 1
#define REPLAY_PACKETS_BY_IDS_COMMAND_TYPE 2

using namespace std;


class PacketLoggerClient : public Client {

private:
	void serializeWrappedPacketDataObject(int command, void* obj, char* serialized, int* len);
	void serializeGetPacketById(int command, void* obj, char* serialized, int* len);
	void serializeReplayPacketsByIds(int command, void* obj, char* serialized, int* len);

	// mocked client for debug
	static WrappedPacketData* preparePacketLoggerClientTest1();
	static WrappedPacketData* preparePacketLoggerClientTest2();
	static WrappedPacketData* preparePacketLoggerClientTest3();
	static WrappedPacketData* preparePacketLoggerClientTest4();
	static void runPacketLoggerClientTest(PacketLoggerClient *client, WrappedPacketData* wpd);
	static void* prepareGetPacketTest(uint64_t packId);
	static void getPacket(PacketLoggerClient *client, void* msgToSend);
	static void* prepareReplayPacketsTest1();
	static void* prepareReplayPacketsTest2();
	static void* prepareReplayPacketsTest3();
	static void* prepareReplayPacketsTest4();
	static void replayPackets(PacketLoggerClient *client, void* msgToSend);
	// end mocked client


protected:
	void serializeObject(int command, void* obj, char* serialized, int* len);
	void handleReturnValue(int status, char* retVal, int len, int command, void* retValAsObj);

public:
	PacketLoggerClient(int port, char* address) : Client(port, address) {
		// do nothing
	}

	static void runTests(char* address);
 };



#endif /* TCP_CLIENTS_PACKETS_LOGGER_CLIENT_HH_ */
