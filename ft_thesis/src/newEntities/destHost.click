// destHost.click

classifier   :: Classifier(12/8100 /* 802.1Q packets */, -);

FromDevice("h3-eth0")
        -> classifier
//        -> Strip(4) /* strip the tsa vlan */
//        -> ipClassifier
//        -> CheckIPHeader(14, CHECKSUM false)
//        -> tcpClassifier
//        -> Unstrip(4)
        -> preparePacket()
		-> Print()
		-> Discard;

classifier[1] -> Discard;
//ipClassifier[1] -> out;
//tcpClassifier[1] -> out;
