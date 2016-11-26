// PushPacketId.click

classifier   :: Classifier(12/8100 /* 802.1Q packets */, -);
ipClassifier :: Classifier(12/0800 /* IP packets */, -);
//tcpClassifier :: IPClassifier(10.0.0.0/24 and tcp, -);
out          :: Queue -> Print -> ToDevice("h2-eth0");

FromDevice("h2-eth0")
        -> classifier
        -> Strip(4) /* strip the tsa vlan */
        -> ipClassifier
//        -> CheckIPHeader(14, CHECKSUM false)
//        -> tcpClassifier
        -> Unstrip(4)
        -> PacketIdEncap(3)
	-> out;

classifier[1] -> out;
ipClassifier[1] -> out;
//tcpClassifier[1] -> out;
