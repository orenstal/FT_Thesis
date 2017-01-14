"""Custom topology example

Two directly connected switches plus a host for each switch:

				 
				 h2		h3
				 | 		|
   h1 --- s1 --- s2 --- s3 --- s4 --- h4
				/  \
			   s5   s6
			   |	|
			   h5	h6
   
   
   h1 - tcp packet generator
   h2 - wrapped packet client(creates packet id and send to packet logger server [h4])
   h3 - the host that receives the packets that are sent from h1 (the destination host)
   h4 - packet logger server
   h5 - determinant logger server
   h6 - mb 1 (extracts packet id and sends records to determinant logger server [h5])

Adding the 'topos' dict with a key/value pair to generate our newly defined
topology enables one to pass in '--topo=mytopo' from the command line.
"""

from mininet.topo import Topo

class MyTopo( Topo ):
	"Simple topology example."
	
	def __init__( self ):
		"Create custom topo."
		
		# Initialize topology
		Topo.__init__( self )
		
		# Add hosts and switches
		packetGenerator = self.addHost( 'h1' )
		wrappedPacketMB = self.addHost( 'h2' )
		destHost = self.addHost( 'h3' )
		packetLoggerServer = self.addHost( 'h4' )
		determinantLoggerServer = self.addHost( 'h5' )
		mb1 = self.addHost( 'h6' )
		s1 = self.addSwitch( 's1' )
		s2 = self.addSwitch( 's2' )
		s3 = self.addSwitch( 's3' )
		s4 = self.addSwitch( 's4' )
		s5 = self.addSwitch( 's5' )
		s6 = self.addSwitch( 's6' )
		
		# Add links
		self.addLink( packetGenerator, s1 )
		self.addLink( s1, s2 )
		self.addLink( s2, wrappedPacketMB )
		self.addLink( s2, s3 )
		self.addLink( s3, destHost )
		self.addLink( s2, s6 )
		self.addLink( s6, mb1 )
		self.addLink( s3, s4 )
		self.addLink( s4, packetLoggerServer )
		self.addLink( s2, s5 )
		self.addLink( s5, determinantLoggerServer )

topos = { 'mytopo': ( lambda: MyTopo() ) }