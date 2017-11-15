// Network topology
//
//       n0 ----------- n1
//            500 Kbps
//             5 ms
//
// - Flow from n0 to n1 using TcpClientServerApplication.
// - Tracing of queues and packet receptions to file "tcp-bulk-send.tr"
//   and pcap tracing available when tracing is turned on.

#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/string.h"
#include "ns3/names.h"
#include "ns3/string.h"

#include "ns3/address-utils.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/udp-socket.h"
#include "ns3/udp-socket-factory.h"

#include "tcp-client-application.h"
#include "tcp-server-application.h"
#include "tcp-client-application-helper.h"
#include "tcp-server-application-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpClientServerExample");

Ipv4InterfaceContainer inetFace;

/************************************************************************************/

TypeId
TcpClientApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpClientApplication")
    .SetParent<Application> ()
    .SetGroupName("Applications") 
    .AddConstructor<TcpClientApplication> ()
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&TcpClientApplication::m_peer),
                   MakeAddressChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&TcpClientApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&TcpClientApplication::m_rxTrace),
                     "ns3::Packet::PacketAddressTracedCallback")
  ;
  return tid;
}


TcpClientApplication::TcpClientApplication ()
  : m_socket (0),
    m_connected (false)
{
  NS_LOG_FUNCTION (this);
  m_totalRx = 0;
}

TcpClientApplication::~TcpClientApplication ()
{
  NS_LOG_FUNCTION (this);
}

uint32_t TcpClientApplication::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

Ptr<Socket>
TcpClientApplication::GetSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

void
TcpClientApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  Application::DoDispose ();
}

// Application Methods
void TcpClientApplication::StartApplication (void) // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  m_tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);

      // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
      if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
          m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
        {
          NS_FATAL_ERROR ("Use TCP instead of UDP.");
        }
      if (InetSocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind ();
        }

      m_socket->Connect (m_peer);
      //m_socket->ShutdownRecv ();
      m_socket->SetConnectCallback (
        MakeCallback (&TcpClientApplication::ConnectionSucceeded, this),
        MakeCallback (&TcpClientApplication::ConnectionFailed, this));
      m_socket->SetSendCallback (
        MakeCallback (&TcpClientApplication::DataSend, this));
      m_socket->SetRecvCallback (
        MakeCallback (&TcpClientApplication::HandleRead, this));
    }
  if (m_connected)
    {
      SendData ();
    }
}

void TcpClientApplication::StopApplication (void) // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_connected = false;
    }
  else
    {
      NS_LOG_WARN ("TcpClientApplication found null socket to close in StopApplication");
    }
}


// Private helpers

void TcpClientApplication::SendData (void)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());
  Ptr<Packet> packet = Create<Packet> (100);
  m_txTrace (packet);
  m_socket->Send (packet);

  HandleRead(m_socket);
  
  //m_socket->Close ();
  //m_connected = false;
}

void TcpClientApplication::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () == 0)
        { //EOF
          break;
        }
      m_totalRx += packet->GetSize ();
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                       << "s packet sink received "
                       <<  packet->GetSize () << " bytes from "
                       << InetSocketAddress::ConvertFrom(from).GetIpv4 ()
                       << " port " << InetSocketAddress::ConvertFrom (from).GetPort ()
                       << " total Rx " << m_totalRx << " bytes");
        }
      m_rxTrace (packet, from);
      socket->SendTo(packet, 0, from);
    }
}

void TcpClientApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("TcpClientApplication Connection succeeded");
  m_connected = true;
  SendData ();
}

void TcpClientApplication::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("TcpClientApplication, Connection Failed");
}

void TcpClientApplication::DataSend (Ptr<Socket>, uint32_t)
{
  NS_LOG_FUNCTION (this);

  if (m_connected)
    { // Only send new data if the connection has completed
      SendData ();
    }
}

/************************************************************************************/

TcpClientApplicationHelper::TcpClientApplicationHelper (Address address)
{
  m_factory.SetTypeId (TcpClientApplication::GetTypeId());
  SetAttribute ("Remote", AddressValue (address));
}

void
TcpClientApplicationHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
TcpClientApplicationHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
TcpClientApplicationHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
TcpClientApplicationHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
TcpClientApplicationHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<Application> ();
  node->AddApplication (app);

  return app;
}

/************************************************************************************/

TypeId 
TcpServerApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpServerApplication")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<TcpServerApplication> ()
    .AddAttribute ("Local",
                   "The Address on which to Bind the rx socket.",
                   AddressValue (),
                   MakeAddressAccessor (&TcpServerApplication::m_local),
                   MakeAddressChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&TcpServerApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&TcpServerApplication::m_rxTrace),
                     "ns3::Packet::PacketAddressTracedCallback")
  ;
  return tid;
}

TcpServerApplication::TcpServerApplication ()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_totalRx = 0;
}

TcpServerApplication::~TcpServerApplication()
{
  NS_LOG_FUNCTION (this);
}

uint32_t TcpServerApplication::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

Ptr<Socket>
TcpServerApplication::GetListeningSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

std::list<Ptr<Socket> >
TcpServerApplication::GetAcceptedSockets (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socketList;
}

void TcpServerApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socketList.clear ();
  Application::DoDispose ();
}


// Application Methods
void TcpServerApplication::StartApplication ()    // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  m_tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      m_socket->Bind (m_local);
      m_socket->Listen ();
      //m_socket->ShutdownSend ();
    }

  m_socket->SetRecvCallback (MakeCallback (&TcpServerApplication::HandleRead, this));
  m_socket->SetAcceptCallback (
    MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
    MakeCallback (&TcpServerApplication::HandleAccept, this));
  m_socket->SetCloseCallbacks (
    MakeCallback (&TcpServerApplication::HandlePeerClose, this),
    MakeCallback (&TcpServerApplication::HandlePeerError, this));
}

void TcpServerApplication::StopApplication ()     // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
  while(!m_socketList.empty ()) //these are accepted sockets, close them
    {
      Ptr<Socket> acceptedSocket = m_socketList.front ();
      m_socketList.pop_front ();
      acceptedSocket->Close ();
    }
  if (m_socket) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void TcpServerApplication::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet, pktSend;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () == 0)
        { //EOF
          break;
        }
      m_totalRx += packet->GetSize ();
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                       << "s packet sink received "
                       <<  packet->GetSize () << " bytes from "
                       << InetSocketAddress::ConvertFrom(from).GetIpv4 ()
                       << " port " << InetSocketAddress::ConvertFrom (from).GetPort ()
                       << " total Rx " << m_totalRx << " bytes");
        }
      m_rxTrace (packet, from);
      pktSend = Create<Packet> (200);
      m_txTrace (pktSend);
      socket->SendTo(pktSend, 0, from);
    }
}


void TcpServerApplication::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}
 
void TcpServerApplication::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}
 

void TcpServerApplication::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  s->SetRecvCallback (MakeCallback (&TcpServerApplication::HandleRead, this));
  //s->SetSendCallback (MakeCallback (&TcpServerApplication::DataSend, this));
  m_socketList.push_back (s);
}

/************************************************************************************/

TcpServerApplicationHelper::TcpServerApplicationHelper (Address address)
{
  m_factory.SetTypeId (TcpServerApplication::GetTypeId());
  SetAttribute ("Local", AddressValue (address));
}

void 
TcpServerApplicationHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
TcpServerApplicationHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
TcpServerApplicationHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
TcpServerApplicationHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
TcpServerApplicationHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<Application> ();
  node->AddApplication (app);

  return app;
}

/************************************************************************************/

void changeAddress(Ptr<Node> node, ApplicationContainer app)
{
  std::pair< Ptr<Ipv4>, uint32_t > face = inetFace.Get(0);
  node->GetObject<Ipv4>()->RemoveAddress(face.second,"10.1.1.1");
  node->GetObject<Ipv4>()->AddAddress(face.second,
                                   Ipv4InterfaceAddress(Ipv4Address("10.1.1.3"),
                                   Ipv4Mask("255.255.255.0")));
  printf(">>> IP changed <<<\n");
}

int
main (int argc, char *argv[])
{

  bool tracing = true;
  uint32_t maxBytes = 100;

//
// Allow the user to override any of the defaults at
// run-time, via command-line arguments
//
  CommandLine cmd;
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("maxBytes",
                "Total number of bytes for application to send", maxBytes);
  cmd.Parse (argc, argv);

//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (2);

  NS_LOG_INFO ("Create channels.");

//
// Explicitly create the point-to-point link required by the topology (shown above).
//
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("500Kbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

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
  inetFace = ipv4.Assign (devices);

  NS_LOG_INFO ("Create Applications.");

//
// Create a TcpClientApplication and install it on node 0
//
  uint16_t port = 9;  // well-known echo port number


  TcpClientApplicationHelper client (InetSocketAddress (inetFace.GetAddress (1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (0.0));
  clientApps.Stop (Seconds (10.0));

//
// Create a TcpServerApplicationApplication and install it on node 1
//
  TcpServerApplicationHelper server (InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer serverApps = server.Install (nodes.Get (1));
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

  //Simulator::Schedule(Seconds(6.0), &changeAddress, nodes.Get (0), clientApps.Get(0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  Ptr<TcpServerApplication> sink1 = DynamicCast<TcpServerApplication> (serverApps.Get (0));
  std::cout << "Total Bytes Received: " << sink1->GetTotalRx () << std::endl;

  Ptr<TcpClientApplication> sink2 = DynamicCast<TcpClientApplication> (clientApps.Get (0));
  std::cout << "Total Bytes Received: " << sink2->GetTotalRx () << std::endl;
}
