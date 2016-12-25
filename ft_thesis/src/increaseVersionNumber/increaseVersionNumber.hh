/*
 * increaseVersionId.h
 *
 *  Created on: Nov 29, 2016
 *      Author: Tal
 */

#ifndef INCREASEVERSIONNUMBER_INCREASEVERSIONNUMBER_HH_
#define INCREASEVERSIONNUMBER_INCREASEVERSIONNUMBER_HH_

#include <click/element.hh>
CLICK_DECLS
class IncreaseVersionNumber : public Element { public:
	IncreaseVersionNumber() CLICK_COLD;
	~IncreaseVersionNumber() CLICK_COLD;

	const char *class_name() const	{ return "increaseVersionNumber"; }
	const char *port_count() const	{ return PORTS_1_1; }

	Packet *smaction(Packet *p);
	void push(int port, Packet *p);
	Packet *pull(int port);
};




#endif /* INCREASEVERSIONNUMBER_INCREASEVERSIONNUMBER_HH_ */
