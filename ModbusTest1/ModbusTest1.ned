import inet.networks.base.TsnNetworkBase;
import inet.node.ethernet.EthernetLink;
import inet.node.tsn.TsnDevice;
import inet.node.tsn.TsnSwitch;


network ModbusTest1 extends TsnNetworkBase
{
    parameters:
        int numClients;
    submodules:
        operatorStation: TsnDevice {
            @display("p=494,71");
        }
        switch1: TsnSwitch {
            @display("p=550,200");
        }
        switch2: TsnSwitch {
            @display("p=750,200");
        }
        server: TsnDevice {
            @display("p=834,71");
        }
        client[numClients]: TsnDevice {
            @display("p=652,329,r,80");
        }
    connections:
        operatorStation.ethg++ <--> EthernetLink <--> switch1.ethg++;
        operatorStation.ethg++ <--> EthernetLink <--> switch2.ethg++;
        switch1.ethg++ <--> EthernetLink <--> switch2.ethg++;
        switch1.ethg++ <--> EthernetLink <--> server.ethg++;
        switch2.ethg++ <--> EthernetLink <--> server.ethg++;

        for i=0..numClients-1 {
            client[i].ethg++ <--> EthernetLink <--> switch1.ethg++;
            client[i].ethg++ <--> EthernetLink <--> switch2.ethg++;
        }
}
