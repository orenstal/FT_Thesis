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


class PALSManager {
	private:
		pthread_mutex_t gpal_mutex;
		pthread_mutex_t spal_mutex;
		int gpal_index;
		int spal_index;
		spal spalList[PALS_SIZE];
		gpal gpalList[PALS_SIZE];

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

		gpal* getGPalList() {
			return gpalList;
		}
};


#endif /* PAL_API_PALS_MANAGER_HH_ */
