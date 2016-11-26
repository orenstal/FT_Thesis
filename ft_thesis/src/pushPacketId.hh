/*
 * PushPacketId.hh
 *
 *  Created on: Nov 19, 2016
 *      Author: Tal
 */

#ifndef PUSHPACKETID_HH_
#define PUSHPACKETID_HH_

//#include <click/element.hh>
#include <stdint.h>
//CLICK_DECLS
class PushPacketId {
	uint64_t _seqNum;
	uint8_t _producerId;

public:
	PushPacketId(uint8_t);
	bool isValidSeqNum(uint32_t);
	uint32_t getNexSeqNum();
	uint16_t createInnerVlan(uint32_t);
	uint16_t createMiddleVlan(uint32_t);
	uint16_t createOuterVlan(uint32_t);
	uint64_t createId();
	const uint32_t MAX_SEQ_NUM = 268435456; // (2^28 -1) - there are 28 (36-5-3) bits for sequence numbers

//private:
//	uint32_t getNexSeqNum();

//#ifdef HAVE_INT64_TYPES
//    typedef uint64_t seqNum_t;
//#else
//    typedef uint32_t seqNum_t;
//#endif


//    uint32_t MAX_SEQ_NUM;

};
//CLICK_ENDDECLS
#endif /* PUSHPACKETID_HH_ */
