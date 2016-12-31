/*
 * pals_manager.hh
 *
 *  Created on: Dec 22, 2016
 *      Author: Tal
 */

#ifndef PAL_API_PALS_MANAGER_HH_
#define PAL_API_PALS_MANAGER_HH_

#include <pthread.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>

/*
 * pal_manager.hh -- Manages pals per packet.
 */

#define PALS_SIZE 10
#define GPAL_VAL_SIZE 50
using namespace std;

typedef struct gpal {
    uint16_t var_id;
    char val[GPAL_VAL_SIZE];
} gpal;

typedef struct spal {
    uint16_t var_id;
    uint32_t seq_num;
} spal;

void serializeGPal(gpal* gpal, char* serializedMsg) {
	uint16_t *q = (uint16_t*)serializedMsg;
	*q = gpal->var_id;
	q++;

	char *p = (char*)q;
	char* gpalValue = gpal->val;

	for (int i=0; i< GPAL_VAL_SIZE; i++, p++) {
		*p = gpalValue[i];
	}
}

static void deserializeGPal(char* serializedMsg, gpal* gpal) {
	uint16_t *q = (uint16_t*)serializedMsg;
	gpal->var_id = *q;
	q++;

	char *p = (char*)q;
	char* gpalValue = gpal->val;

	for (int i=0; i< GPAL_VAL_SIZE; i++, p++) {
		gpalValue[i] = *p;
	}
}

static void serializeSPal(spal* spal, char* serializedMsg) {
	uint16_t *q = (uint16_t*)serializedMsg;
	*q = spal->var_id;
	q++;

	uint32_t *p = (uint32_t*)q;
	*p = spal->seq_num;
}

static void deserializeSPal(char* serializedMsg, spal* spal) {
	uint16_t *q = (uint16_t*)serializedMsg;
	spal->var_id = *q;
	q++;

	uint32_t *p = (uint32_t*)q;
	spal->seq_num = *p;
}


class PALSManager {
	private:
		pthread_mutex_t gpal_mutex;
		pthread_mutex_t spal_mutex;
		uint16_t gpal_index;
		uint16_t spal_index;
		spal spalList[PALS_SIZE];
		gpal gpalList[PALS_SIZE];
		uint16_t middleboxId;
		uint64_t packetId;

	public:
		PALSManager() {
			memset(this, 0, sizeof(*this));
			pthread_mutex_init(&gpal_mutex, NULL);
			pthread_mutex_init(&spal_mutex, NULL);
			memset(spalList, 0, sizeof(spal)*PALS_SIZE);
			memset(gpalList, 0, sizeof(gpal)*PALS_SIZE);
			gpal_index = 0;
			spal_index = 0;
		}

		PALSManager(uint16_t mbId, uint64_t packId) {
			PALSManager();
			middleboxId = mbId;
			packetId = packId;
		}

		~PALSManager() {
			pthread_mutex_destroy(&spal_mutex);
			pthread_mutex_destroy(&gpal_mutex);
		}

		void addSPal(spal* spal_instance) {
			pthread_mutex_lock(&spal_mutex);
			spalList[spal_index] = *spal_instance;
			spal_index++;
			pthread_mutex_unlock(&spal_mutex);
		}

		void addGPal(gpal* gpal_instance) {
			pthread_mutex_lock(&gpal_mutex);
			gpalList[gpal_index] = *gpal_instance;
			gpal_index++;
			pthread_mutex_unlock(&gpal_mutex);
		}

		void createSPalAndAdd(uint16_t var_id, uint32_t seq_num) {
			spal sp = {var_id, seq_num};
			addSPal(&sp);
		}

		void createGPalAndAdd(uint16_t var_id, const char* val) {
			gpal gp;

			memcpy(gp.val, val, GPAL_VAL_SIZE);
			gp.var_id = var_id;

			addGPal(&gp);
		}

		spal* getSPalList() {
			return spalList;
		}

		void setSPalList(spal* spals) {
			for (uint16_t i=0; i<PALS_SIZE; i++) {
				spalList[i] = spals[i];
			}
		}

		gpal* getGPalList() {
			return gpalList;
		}

		void setGPalList(gpal* gpals) {
			for (uint16_t i=0; i<PALS_SIZE; i++) {
				gpalList[i] = gpals[i];
			}
		}

		uint16_t getSPalSize() {
			return spal_index;
		}

		void setSPalSize(uint16_t spalSize) {
			spal_index = spalSize;
		}

		uint16_t getGPalSize() {
			return gpal_index;
		}

		void setGPalSize(uint16_t gpalSize) {
			gpal_index = gpalSize;
		}

		uint16_t getMBId() {
			return middleboxId;
		}

		void setMBId(uint16_t mbId) {
			middleboxId = mbId;
		}

		uint64_t getPacketId() {
			return packetId;
		}

		void setPacketId(uint64_t packId) {
			packetId = packId;
		}

		void printContent() {
			int gpalSize = getGPalSize();
			int spalSize = getSPalSize();

			cout << "-------------------------------------" << endl;
			cout << "mbId: " << middleboxId << "\n";
			cout << "packetId: " << packetId << "\n";
			cout << "gpal list size: " << gpalSize << "\n";

			for (int i=0; i<gpalSize; i++) {
				cout << "gpal[" << i << "]\t" << gpalList[i].var_id << "\t";
				string val = string(gpalList[i].val);
				cout << val << endl;
			}

			cout << "\nspal list size: " << spalSize << "\n";

			for (int i=0; i<spalSize; i++) {
				cout << "spal[" << i << "]\t" << spalList[i].var_id << "\t" << spalList[i].seq_num << endl;
			}

			cout << "-------------------------------------" << endl;

		}

		static void serialize(PALSManager* pm, char* serializedMsg, int* len) {
			uint16_t *q = (uint16_t*)serializedMsg;
			*q = pm->getMBId();
			q++;

			uint64_t *p = (uint64_t*)q;
			*p = pm->getPacketId();
			p++;

			char* r = serializeGPals(pm, (char*)p);
			serializeSPals(pm, r);

			*len = getSerializationLength(pm);
		}

		static uint16_t getSerializationLength(PALSManager* pm) {
			uint16_t len;
			// mbId
			len = sizeof(uint16_t);

			// packetId
			len+= sizeof(uint64_t);

			// actual gpal_list size
			len+= sizeof(uint16_t);

			// actual gpals
			len+= pm->getGPalSize() * sizeof(gpal);

			// actual spal_list size
			len+= sizeof(uint16_t);

			// actual spals
			len+= pm->getSPalSize() * sizeof(spal);

			cout << "serialization length is: " << len << endl;
			return len;
		}

		static char* serializeGPals(PALSManager* pm, char* serializedMsg) {
			uint16_t *q = (uint16_t*)serializedMsg;
			uint16_t gpalListSize = pm->getGPalSize();
			*q = gpalListSize;
			q++;

			gpal* gpalList = pm->getGPalList();

			gpal *p = (gpal*)q;

			for (int i=0; i<gpalListSize; i++, p++) {
				::serializeGPal(&gpalList[i], (char*)p);
			}

			return (char*)p;
		}

		static char* serializeSPals(PALSManager* pm, char* serializedMsg) {
			uint16_t *q = (uint16_t*)serializedMsg;
			uint16_t spalListSize = pm->getSPalSize();
			*q = spalListSize;
			q++;

			spal* spalList = pm->getSPalList();

			spal *p = (spal*)q;

			for (int i=0; i<spalListSize; i++, p++) {
				::serializeSPal(&spalList[i], (char*)p);
			}

			return (char*)p;
		}


		static void deserialize(char* serializedMsg, PALSManager* pm) {
			uint16_t *q = (uint16_t*)serializedMsg;
			pm->setMBId(*q);
			q++;

			uint64_t *p = (uint64_t*)q;
			pm->setPacketId(*p);
			p++;

			char* r = deserializeGPals((char*)p, pm);
			deserializeSPals(r, pm);
		}

		static char* deserializeGPals(char* serializedMsg, PALSManager* pm) {
			uint16_t *q = (uint16_t*)serializedMsg;
			uint16_t gpalListSize = *q;
			pm->setGPalSize(gpalListSize);
			q++;

			gpal* gpalList = pm->getGPalList();

			gpal *p = (gpal*)q;

			for (int i=0; i<gpalListSize; i++, p++) {
				::deserializeGPal((char*)p, &gpalList[i]);
			}

			return (char*)p;
		}

		static char* deserializeSPals(char* serializedMsg, PALSManager* pm) {
			uint16_t *q = (uint16_t*)serializedMsg;
			uint16_t spalListSize = *q;
			pm->setSPalSize(spalListSize);
			q++;

			spal* spalList = pm->getSPalList();

			spal *p = (spal*)q;

			for (int i=0; i<spalListSize; i++, p++) {
				::deserializeSPal((char*)p, &spalList[i]);
			}

			return (char*)p;
		}
};


#endif /* PAL_API_PALS_MANAGER_HH_ */
