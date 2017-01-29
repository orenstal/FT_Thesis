// mb2.click 	--	mb2 (slave of mb1)

classifier   :: Classifier(12/8100 /* 802.1Q packets */, -);
ipClassifier :: Classifier(12/0800 /* IP packets */, -);
out          :: Queue -> ToDevice("h7-eth0");
pp           :: preparePacket(false);
ivn          :: increaseVersionNumber(false, false);
dpr          :: distributePacketRecords(2, false);	/*this element should be the last one before out or discard elements!!*/
pp1          :: preparePacket(false);
dpr1         :: distributePacketRecords(2, false);
pp2          :: preparePacket(false);
ivn2         :: increaseVersionNumber(false, false);
dpr2         :: distributePacketRecords(2, false);


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
	-> Strip(16) /* strip the 4 tsa vlan layers: 1 for routing, 3 for packet id*/
	-> ipClassifier
	-> Unstrip(16)
	-> Print()
	-> pp2
	-> ivn2
	-> dpr2 /*this element should be the last one before out or discard elements!!*/
	-> out;

classifier[1] -> out;
ipClassifier[1] -> out;
