// mb2.click 	--	mb2 (slave of mb1)

classifier   :: Classifier(12/8100 /* 802.1Q packets */, -);
ipClassifier :: Classifier(12/0800 /* IP packets */, -);
//tcpClassifier :: IPClassifier(10.0.0.0/24 and tcp, -);
out          :: Queue -> ToDevice("h7-eth0");
pp           :: preparePacket(false);
ivn          :: increaseVersionNumber(false, false);
dpr          :: distributePacketRecords(1, false)	/*this element should be the last one before out or discard elements!!*/


Socket(TCP, 10.0.0.7, 9999)	// listen to incoming socket on port 9999
	-> Print()
	-> pp
	-> ivn 
	-> dpr;

FromDevice("h7-eth0")
	-> classifier
	-> Strip(16) /* strip the 4 tsa vlan layers: 1 for routing, 3 for packet id*/
	-> ipClassifier
//        -> CheckIPHeader(14, CHECKSUM false)
//        -> tcpClassifier
	-> Unstrip(16)
	-> Print()
	-> pp 
	-> ivn 
	-> dpr /*this element should be the last one before out or discard elements!!*/
//	-> setIdAnno
	-> out;

classifier[1] -> out;
ipClassifier[1] -> out;
//tcpClassifier[1] -> out;