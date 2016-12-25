/*
 * t.cpp
 *
 *  Created on: Dec 11, 2016
 *      Author: Tal
 */
#include "pals_manager.hh"
#include <string>
#include <iostream>

typedef struct gpal gpal;
typedef struct spal spal;
using namespace std;


class PALSManagerTest {
	public:
		PALSManagerTest() {
		}

		void runTest() {
			PALSManager* pm = new PALSManager();
			gpal gp_1 = {1, "first_gpal_1"};
			spal sp_1 = {1, 1};
			spal sp_3 = {1, 3};
			spal sp_4 = {2, 1};
			gpal gp_3 = {2, "first_gpal_2"};

			pm->addGPal(&gp_1);
			pm->addSPal(&sp_1);
			pm->createSPalAndAdd(1,2);
			pm->addSPal(&sp_3);
			pm->createGPalAndAdd(1, "second_gpal_1");
			pm->addSPal(&sp_4);
			pm->addGPal(&gp_3);


			gpal* gpalList = pm->getGPalList();
			spal* spalList = pm->getSPalList();

			// extract and compare the first gpal
			compareGpals(&gp_1, &gpalList[0]);

			// extract and compare the second gpal
			gpal gp_2 = {1, "second_gpal_1"};
			compareGpals(&gp_2, &gpalList[1]);

			// extract and compare the first gpal
			compareGpals(&gp_3, &gpalList[2]);

			// extract and compare the first spal
			compareSpals(&sp_1, &spalList[0]);

			// extract and compare the second spal
			spal sp_2 = {1, 2};
			compareSpals(&sp_2, &spalList[1]);

			// extract and compare the third spal
			compareSpals(&sp_3, &spalList[2]);

			// extract and compare the fourth spal
			compareSpals(&sp_4, &spalList[3]);

			delete pm;
		}

		bool compareGpals(gpal* first, gpal* second) {
			bool succeed = true;
			if (first->var_id != second->var_id) {
				succeed = false;
			} else if (strcmp(first->val, second->val) != 0) {
				succeed = false;
			}

			if (succeed) {
				cout << "succeed! var_id: " << first->var_id << ", val: " << second->val << endl;
			} else {
				cout << "failed! var_id: " << first->var_id << ", val: " << second->val << endl;
			}

			return succeed;
		}

		bool compareSpals(spal* first, spal* second) {
			bool succeed = true;
			if (first->var_id != second->var_id) {
				succeed = false;
			} else if (first->seq_num != second->seq_num) {
				succeed = false;
			}

			if (succeed) {
				cout << "succeed! var_id: " << first->var_id << ", seq: " << second->seq_num << endl;
			} else {
				cout << "failed! var_id: " << first->var_id << ", seq: " << second->seq_num << endl;
			}

			return succeed;
		}
};

int main() {
	PALSManagerTest* palsManagerTest = new PALSManagerTest();
	palsManagerTest->runTest();
	return 0;
}
