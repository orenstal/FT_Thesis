/*
 * replay_packets.hh
 *
 *  Created on: Jan 20, 2017
 *      Author: Tal
 */

#ifndef COMMON_REPLAYPACKETS_REPLAY_PACKETS_HH_
#define COMMON_REPLAYPACKETS_REPLAY_PACKETS_HH_

#define MAX_ADDRESS_LEN 16

#include <vector>

typedef struct ReplayPackets {
	uint16_t mbId;
	uint16_t port;
	char address[MAX_ADDRESS_LEN];
	uint8_t addressLen;
    vector<uint64_t>* packetIds;
} ReplayPackets;



#endif /* COMMON_REPLAYPACKETS_REPLAY_PACKETS_HH_ */
