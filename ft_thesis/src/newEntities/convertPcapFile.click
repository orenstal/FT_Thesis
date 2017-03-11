// convertPcapFile.click

// This configuration converts alexa pcap to our network (change src and dest mac and ip addresses).
// Note that we don't need to push vlan because the tsa does so.

// Run it at user level with
// 'click convertPcapFile.click'


ipClassifier :: Classifier(12/0800 /* IP packets */, -);

FromDump("~/alexa_tcp80.pcap")
    -> StoreEtherAddress(00:00:00:00:00:01, 6)
    -> StoreEtherAddress(00:00:00:00:00:03, 0)
	-> ipClassifier
	-> Queue
	-> MarkIPHeader(14)
	->StoreIPAddress(10.0.0.1, src)
	->StoreIPAddress(10.0.0.3, dst)
	-> ToDump("~/new_alexa_tcp80.pacp");

ipClassifier[1]->Discard;
