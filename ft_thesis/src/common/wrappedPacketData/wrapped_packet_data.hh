/*
 * wrapped_packet_data.hh
 *
 *  Created on: Jan 1, 2017
 *      Author: Tal
 */

#ifndef COMMON_WRAPPEDPACKETDATA_WRAPPED_PACKET_DATA_HH_
#define COMMON_WRAPPEDPACKETDATA_WRAPPED_PACKET_DATA_HH_

typedef struct WrappedPacketData {
    uint64_t packetId;
    uint16_t offset;
    uint16_t size;
    char* data;
} WrappedPacketData;



#endif /* COMMON_WRAPPEDPACKETDATA_WRAPPED_PACKET_DATA_HH_ */
