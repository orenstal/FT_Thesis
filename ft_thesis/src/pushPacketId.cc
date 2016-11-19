/*
 * pushPacketId.cc
 *
 *  Created on: Nov 19, 2016
 *      Author: Tal
 */


//#include <click/config.h>
//#include <click/straccum.hh>
//#include <click/args.hh>
#include "pushPacketId.hh"
#include <iostream>
using namespace std;
//CLICK_DECLS

PushPacketId::PushPacketId(uint8_t producerID) {
	_seqNum = 128;
	_producerId = producerID;	// valid values for producer id are 1-6
}

bool PushPacketId::isValidSeqNum(uint32_t candidate) {
	// we want to ignore the seven right-most bits (because they part of the inner vlan and
	// therefore valid (no chance to get 0 or 4095 thanks for the version bits).
	candidate = candidate >> 7;

	// then we want to check the 12 bits that constitute the second vlan (4095 = 111111111111).
	candidate &= 4095;

	// finally we check that the middle vlan isn't 0 or 4095 (preserved for 802.1Q).
	if (candidate == 4095 || candidate == 0) {
		return false;
	}

	return true;
}

// sequence number constitutes of 2^28 -1 bits (except 3 bits for producer id and 5 bits for version).
// Overall we have 36 bits (12 for each vlan level).
uint32_t PushPacketId::getNexSeqNum() {
	bool isValid = false;

	while (!isValid) {
		if (_seqNum > MAX_SEQ_NUM) {
			_seqNum = 128;	// the first valid sequence valid
//			cout << "start again... MAX_SEQ_NUM: " << MAX_SEQ_NUM << endl;
		}

		if (isValidSeqNum(_seqNum)) {
			isValid = true;
		} else {
			_seqNum += 128;
		}
	}

	return _seqNum++;
}


uint16_t PushPacketId::createInnerVlan(uint32_t seqNumber) {
	uint16_t temp;

	// extract the 9 left-most bits among the 28 bits (although there are 32 bits, only the first 28 are in use)
	temp = seqNumber >> 19;

	// concatenate the 9 bits from temp to the 3 digits from _producerId.
	return (_producerId << 9) | temp;
}


uint16_t PushPacketId::createMiddleVlan(uint32_t seqNumber) {
	uint16_t temp;

	// remove the 7 right-most bits (they are part of the inner vlan)
	temp = seqNumber >> 7;

	// extract the 12 digits of the middle vlan. 524160 = (00000000000000000000111111111111).
	return (temp & 4095);
}


uint16_t PushPacketId::createOuterVlan(uint32_t seqNumber) {
	uint16_t temp;

	// extract the 7 right-most bits
	temp = seqNumber & 127;

	// concatenate 5 zero's for initial version (valid values are 1-30)
	temp = (temp << 5);

	return (temp | 1);
}



uint64_t PushPacketId::createId() {

	// nextSeqNum constitute of at most 28 bits.
	uint32_t nextSeqNum = getNexSeqNum();

	uint64_t innerVlan = createInnerVlan(nextSeqNum);
	uint64_t middleVlan = createMiddleVlan(nextSeqNum);
	uint64_t outerVlan = createOuterVlan(nextSeqNum);

	uint64_t unified = (innerVlan << 24) | (middleVlan << 12) | outerVlan;

	cout << "unified id: " << unified << ", seqNum: " << nextSeqNum << ", inner: " << innerVlan << ", middle: " << middleVlan << ", outer: " << outerVlan << endl;

	return unified;
}

//CLICK_ENDDECLS
//EXPORT_ELEMENT(SimpleCounter)

