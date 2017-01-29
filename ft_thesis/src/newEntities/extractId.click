// PushPacketId.click

classifier   :: Classifier(12/8100 /* 802.1Q packets */, -);
//ipClassifier :: Classifier(12/0800 /* IP packets */, -);
//tcpClassifier :: IPClassifier(10.0.0.0/24 and tcp, -);
//out          :: Queue -> Print -> ToDevice("h3-eth0");

FromDevice("h3-eth0")
        -> classifier
//        -> Strip(4) /* strip the tsa vlan */
//        -> ipClassifier
//        -> CheckIPHeader(14, CHECKSUM false)
//        -> tcpClassifier
//        -> Unstrip(4)
//        -> setIdAnno
        -> preparePacket()
        -> Print()
        -> increaseVersionNumber
        -> setIdAnno
        -> Print()
	-> Discard;

classifier[1] -> Discard;
//ipClassifier[1] -> out;
//tcpClassifier[1] -> out;
