import inet.networks.base.TsnNetworkBase;
import inet.node.ethernet.EthernetLink;
import inet.node.tsn.TsnDevice;
import inet.node.tsn.TsnSwitch;
import inet.node.inet.StandardHost;
import inet.node.ethernet.Eth100M;


network ExtInterfaceTest extends TsnNetworkBase
{
    submodules:

        server: StandardHost {
            @display("p=449,194");
        }
//        slave: StandardHost {
//            @display("p=200,194");
//        }
	connections allowunconnected:
//	    server.ethg++ <--> Eth100M <--> slave.ethg++;

}

