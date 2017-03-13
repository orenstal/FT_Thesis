// mb7.click 	--	mb7 (slave of mb6)

classifier   :: Classifier(12/8100 /* 802.1Q packets */, -);
ipClassifier :: Classifier(12/0800 /* IP packets */, -);
out          :: Queue -> ToDevice("h7-eth0");
pp           :: preparePacket(6, false);
ivn          :: increaseVersionNumber(false, false);
dpr          :: distributePacketRecords(7, false);	/*this element should be the last one before out or discard elements!!*/
pp1          :: preparePacket(6, false);
dpr1         :: distributePacketRecords(7, false);
pp2          :: preparePacket(7, true, 0);
ivn2         :: increaseVersionNumber(true, false);
dpr2         :: distributePacketRecords(7, true);
pp3          :: preparePacket(7, true, 0);
dpr3         :: distributePacketRecords(7, true);


Socket(TCP, 10.0.0.7, 9999)	// listen to incoming socket on port 9999
	-> Print()
	-> pp
//	-> aa
//	-> ivn 
	-> dpr
	-> pp1
	-> ivn
	-> dpr1
	-> Discard;


FromDevice("h7-eth0")
	-> classifier
	-> Strip(12) /* strip the 4 tsa vlan layers for packet id*/
	-> ipClassifier
	-> Unstrip(12)
	-> pp2
	-> Print()
	-> dpr2
	-> pp3
	-> ivn2
	-> dpr3 /*this element should be the last one before out or discard elements!!*/
	-> out;

classifier[1] -> out;
ipClassifier[1] -> out;
