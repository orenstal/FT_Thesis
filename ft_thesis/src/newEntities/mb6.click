// mb6.click 	--	mb6 (master of mb7)

classifier   :: Classifier(12/8100 /* 802.1Q packets */, -);
ipClassifier :: Classifier(12/0800 /* IP packets */, -);
//tcpClassifier :: IPClassifier(10.0.0.0/24 and tcp, -);
out          :: Queue -> ToDevice("h6-eth0");
pp           :: preparePacket(6, true, 0);
ivn          :: increaseVersionNumber(true, false);
dpr          :: distributePacketRecords(6, true)	/*this element should be the last one before out or discard elements!!*/
pp1          :: preparePacket(6, true, 0);
dpr1         :: distributePacketRecords(6, true);

FromDevice("h6-eth0")
	-> classifier
	-> Strip(12) /* strip the 3 tsa vlan layers for packet id*/
	-> ipClassifier
	-> Unstrip(12)
	-> pp
	-> Print()
	-> dpr	/*this element should be the last one before out or discard elements!!*/
	-> pp1
	-> ivn
	-> dpr1
//	-> setIdAnno
	-> out;

classifier[1] -> out;
ipClassifier[1] -> out;
//tcpClassifier[1] -> out;
