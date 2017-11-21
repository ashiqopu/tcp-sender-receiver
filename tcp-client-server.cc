//
// Network topology
//
//  n0
//     \ 1 Mb/s, 5ms
//      \          1Mb/s, 5ms
//       n2 -------------------------n3
//      /
//     / 1 Mb/s, 5ms
//   n1
//
// - Flow from n0 to n1 using TcpClientServerApplication.
// - Tracing of queues and packet receptions to file "tcp-client-server.tr"
//   and pcap tracing available when tracing is turned on.

#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"

#include "ns3/inet-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/string.h"
#include "ns3/names.h"
#include "ns3/string.h"

#include "ns3/address-utils.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/udp-socket.h"
#include "ns3/udp-socket-factory.h"

#include "tcp-server-application.h"
#include "tcp-client-application.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpClientServerExample");

uint16_t port = 9;  // well-known echo port number
double firstIPchange = 0.3;
ApplicationContainer clientApps;
ApplicationContainer serverApps;

/************************************************************************************/

void dynamicClient(Ptr<Node> node, int id, Ipv4Address servAddress,
									 Ipv4InterfaceContainer inetFace, bool ipChanged)
{
  if (Simulator::Now().GetSeconds() > firstIPchange && Simulator::Now().GetSeconds() < (firstIPchange+0.2) && !ipChanged)
  {
    std::cout << "At time " << Simulator::Now().GetSeconds() << std::endl;

    Ptr<TcpServerApplication> sink1 = DynamicCast<TcpServerApplication> (serverApps.Get (0));
    std::cout << "Server Total Bytes Received: "
              << sink1->GetTotalRx () << std::endl;
    
    Ptr<TcpClientApplication> client = DynamicCast<TcpClientApplication> (clientApps.Get (id));
    std::cout << "Client " << id << " Total Bytes Received: " 
              << client->GetTotalRx () << std::endl;

    // Get Ipv4 instance of the node
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    // Get Ipv4InterfaceAddress of xth interface.
    Ipv4Address addrBefore = ipv4->GetAddress (1, 0).GetLocal ();

    std::pair< Ptr<Ipv4>, uint32_t > face = inetFace.Get(0);
    if(id == 0){
    	node->GetObject<Ipv4>()->RemoveAddress(face.second,"10.1.1.1");
    	node->GetObject<Ipv4>()->AddAddress(face.second,
                                     Ipv4InterfaceAddress(Ipv4Address("10.1.1.3"),
                                     Ipv4Mask("255.255.255.0")));
    }
    else if(id == 1){
    	node->GetObject<Ipv4>()->RemoveAddress(face.second,"10.1.2.1");
    	node->GetObject<Ipv4>()->AddAddress(face.second,
                                     Ipv4InterfaceAddress(Ipv4Address("10.1.2.3"),
                                     Ipv4Mask("255.255.255.0")));
    }

    Ipv4Address addrAfter = ipv4->GetAddress (1, 0).GetLocal ();
    
    std::cout << ">>> Client " << id << " IP changed from "
              << addrBefore << " to " << addrAfter << " <<<" << std::endl;
	  
    // connection restart
    client->StartConnection();
    ipChanged = true;
  }

  else if (Simulator::Now().GetSeconds() > 5.0 && ipChanged)
  {
    std::cout << "At time " << Simulator::Now().GetSeconds() << std::endl;

    Ptr<TcpServerApplication> sink1 = DynamicCast<TcpServerApplication> (serverApps.Get (0));
    std::cout << "Server Total Bytes Received: "
              << sink1->GetTotalRx () << std::endl;
    
    Ptr<TcpClientApplication> client = DynamicCast<TcpClientApplication> (clientApps.Get (id));
    std::cout << "Client " << id << " Total Bytes Received: " 
              << client->GetTotalRx () << std::endl;

    // Get Ipv4 instance of the node
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    // Get Ipv4InterfaceAddress of xth interface.
    Ipv4Address addrBefore = ipv4->GetAddress (1, 0).GetLocal ();

    std::pair< Ptr<Ipv4>, uint32_t > face = inetFace.Get(0);
    if(id == 0){
      node->GetObject<Ipv4>()->RemoveAddress(face.second,"10.1.1.3");
      node->GetObject<Ipv4>()->AddAddress(face.second,
                                     Ipv4InterfaceAddress(Ipv4Address("10.1.1.1"),
                                     Ipv4Mask("255.255.255.0")));
    }
    else if(id == 1){
      node->GetObject<Ipv4>()->RemoveAddress(face.second,"10.1.2.3");
      node->GetObject<Ipv4>()->AddAddress(face.second,
                                     Ipv4InterfaceAddress(Ipv4Address("10.1.2.1"),
                                     Ipv4Mask("255.255.255.0")));
    }

    Ipv4Address addrAfter = ipv4->GetAddress (1, 0).GetLocal ();
    
    std::cout << ">>> Client " << id << " IP changed from "
              << addrBefore << " to " << addrAfter << " <<<" << std::endl;
    
    // connection restart
    client->StartConnection();
    ipChanged = false;
  }

  Simulator::Schedule(Seconds(0.1), &dynamicClient, node, id,
  										servAddress, inetFace, ipChanged);
}

int
main (int argc, char *argv[])
{

  bool tracing = true;
  uint32_t maxBytes = (15*1024);

  bool ipChanged[2] = {false};

//
// Allow the user to override any of the defaults at
// run-time, via command-line arguments
//
  CommandLine cmd;
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("maxBytes",
                "Total number of bytes for application to send", maxBytes);
  cmd.AddValue ("firstIPchange",
                "Time for first IP change", firstIPchange);
  cmd.Parse (argc, argv);

//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (4);
  NodeContainer n0n2 = NodeContainer (nodes.Get (0), nodes.Get (2));
  NodeContainer n1n2 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer n3n2 = NodeContainer (nodes.Get (3), nodes.Get (2));

  NS_LOG_INFO ("Create channels.");
//
// Explicitly create the point-to-point link required by the topology (shown above).
//
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));

  NetDeviceContainer d0d2 = pointToPoint.Install (n0n2);
  NetDeviceContainer d1d2 = pointToPoint.Install (n1n2);
  NetDeviceContainer d3d2 = pointToPoint.Install (n3n2);

//
// Install the internet stack on the nodes
//
  InternetStackHelper internet;
  internet.Install (nodes);

//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i2 = ipv4.Assign (d0d2);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign (d1d2);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i2 = ipv4.Assign (d3d2);

  // Create router nodes, initialize routing database and set up the routing
  // tables in the nodes.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Applications.");

//
// Create a TcpClientApplication and install it on node 0
//

  TcpClientApplicationHelper client (InetSocketAddress (i3i2.GetAddress (0), port));
  client.SetAttribute ("MaxRxBytes", UintegerValue (maxBytes));
  clientApps = client.Install (nodes.Get (0));
  clientApps.Add(client.Install (nodes.Get (1)));
  clientApps.Start (Seconds (0.0));
  clientApps.Stop (Seconds (10.0));

  for(int i=0; i<2; i++){
    Ptr<TcpClientApplication> client = DynamicCast<TcpClientApplication> (clientApps.Get (i));
    client->StartConnection();
  }

  Ipv4Address dstaddr ("10.1.3.1");
//
// Create a TcpServerApplicationApplication and install it on node 1
//
  TcpServerApplicationHelper server (InetSocketAddress (Ipv4Address::GetAny (), port));
  server.SetAttribute ("MaxTxBytes", UintegerValue (maxBytes));
  serverApps = server.Install (nodes.Get (3));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

//
// Set up tracing if enabled
//
  if (tracing)
    {
      AsciiTraceHelper ascii;
      pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("tcp-client-server.tr"));
      pointToPoint.EnablePcapAll ("tcp-client-server", false);
    }

//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");

  Simulator::Schedule(Seconds(0.1), &dynamicClient, nodes.Get (0), 0,
                      dstaddr, i0i2, ipChanged[0]);

  Simulator::Schedule(Seconds(0.1), &dynamicClient, nodes.Get (1), 1,
                      dstaddr, i1i2, ipChanged[1]);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  Ptr<TcpServerApplication> sink1 = DynamicCast<TcpServerApplication> (serverApps.Get (0));
  std::cout << "Server Total Bytes Received: "
  					<< sink1->GetTotalRx () << std::endl;
  
  for(int i=0; i<2; i++){
	  Ptr<TcpClientApplication> client = DynamicCast<TcpClientApplication> (clientApps.Get (i));
	  std::cout << "Client " << i << " Total Bytes Received: " 
	  					<< client->GetTotalRx () << std::endl;
  }

  printf("\nAt end of Simulation:\n");
  for(int i=0; i<2; i++){
    Ptr<TcpClientApplication> client = DynamicCast<TcpClientApplication> (clientApps.Get (i));
    std::cout << "Client " << i << " Total Bytes Received: " 
              << client->GetCompleteRx () << std::endl;
  }
}
