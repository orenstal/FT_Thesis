// PushPacketId.click

classifier   :: Classifier(12/8100 /* 802.1Q packets */, -);
ipClassifier :: Classifier(12/0800 /* IP packets */, -);
//tcpClassifier :: IPClassifier(10.0.0.0/24 and tcp, -);
out          :: Queue -> ToDevice("h6-eth0");

FromDevice("h6-eth0")
	-> classifier
	-> Strip(16) /* strip the 4 tsa vlan layers: 1 for routing, 3 for packet id*/
	-> ipClassifier
//        -> CheckIPHeader(14, CHECKSUM false)
//        -> tcpClassifier
	-> Unstrip(16)
	-> preparePacket()
	-> Print()
//	-> ditributePacketRecords(1, false)
	-> increaseVersionNumber
	-> ditributePacketRecords(1, true)	/*this element should be the last one before out or discard elements!!*/
//	-> setIdAnno
	-> out;

classifier[1] -> out;
ipClassifier[1] -> out;
//tcpClassifier[1] -> out;