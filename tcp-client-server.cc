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

//#include "tcp-client-application.h"
#include "tcp-server-application.h"
//#include "tcp-client-application-helper.h"
#include "tcp-server-application-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpClientServerExample");

Ipv4InterfaceContainer inetFace;
uint32_t c_totalRx = 0;
double timeCounter = 0.0;
bool ipChanged = false;
bool connected = false;
bool sv_connected = false;

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
    .AddAttribute ("SendSize", "The amount of data to send each time.",
                   UintegerValue (512),
                   MakeUintegerAccessor (&TcpServerApplication::m_sendSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("MaxBytes",
                   "The total number of bytes to send. "
                   "Once these bytes are sent, "
                   "no data  is sent again. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TcpServerApplication::m_maxBytes),
                   MakeUintegerChecker<uint32_t> ())
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
  m_totBytes = 0;
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

void
TcpServerApplication::SetMaxBytes (uint32_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);
  m_maxBytes = maxBytes;
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

void TcpServerApplication::SendData (Ptr<Socket> sock, Address from)
{
  NS_LOG_FUNCTION (this << sock);

  if(sv_connected)
  {
    while (m_maxBytes == 0 || m_totBytes < m_maxBytes)
      { // Time to send more
        uint32_t toSend = m_sendSize;
        // Make sure we don't send too many
        if (m_maxBytes > 0)
          {
            toSend = std::min (m_sendSize, m_maxBytes - m_totBytes);
          }
        NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());
        Ptr<Packet> packet = Create<Packet> (toSend);
        m_txTrace (packet);
        int actual = sock->SendTo (packet, 0, from);
        if (actual > 0)
          {
            m_totBytes += actual;
          }
        // We exit this loop when actual < toSend as the send side
        // buffer is full. The "DataSent" callback will pop when
        // some buffer space has freed ip.
        if ((unsigned)actual != toSend)
          {
            break;
          }
      }
    }
  // Check if time to close (all sent)
  if (m_totBytes == m_maxBytes && sv_connected)
    {
      sv_connected = false;
      m_totBytes = 0;
      sock->Close ();
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
      if (packet->GetSize () == 13)
        { //EOF
          break;
        }
    }
  if (packet->GetSize () == 13)
  { //EOF
    SendData(socket, from);
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
  sv_connected = true;
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

void HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () == 0)
        { //EOF
          break;
        }
      c_totalRx += packet->GetSize ();
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                       << "s packet sink received "
                       <<  packet->GetSize () << " bytes from "
                       << InetSocketAddress::ConvertFrom(from).GetIpv4 ()
                       << " port " << InetSocketAddress::ConvertFrom (from).GetPort ()
                       << " total Rx " << c_totalRx << " bytes");
        }
    }
}

void SendData(Ptr<Socket> sock, uint32_t i)
{
  if(connected)
  {
    Ptr<Packet> packet = Create<Packet> (13);
    sock->Send(packet);
    HandleRead(sock);
  }
}

void dynamicClient(Ptr<Node> node, Ptr<Socket> clientSock,
                   Ipv4Address servAddress, uint16_t servPort,
                   ApplicationContainer serverApps)
{
  timeCounter += 0.1;
  if (timeCounter > 6.0 && ipChanged == false)
  {
    std::pair< Ptr<Ipv4>, uint32_t > face = inetFace.Get(0);
    node->GetObject<Ipv4>()->RemoveAddress(face.second,"10.1.1.1");
    node->GetObject<Ipv4>()->AddAddress(face.second,
                                     Ipv4InterfaceAddress(Ipv4Address("10.1.1.3"),
                                     Ipv4Mask("255.255.255.0")));

    Ptr<TcpServerApplication> sink1 = DynamicCast<TcpServerApplication> (serverApps.Get (0));
    std::cout << "Server Total Bytes Received: " << sink1->GetTotalRx () << std::endl;
    std::cout << "Client Total Bytes Received: " << c_totalRx << std::endl;

    printf(">>> IP changed <<<\n");
    ipChanged = true;
    connected = false;
    c_totalRx = 0;
    //clientSock->Close();
  }
  if(c_totalRx == 15*1024)
  {
    connected = false;
  }
  else if(connected == false)
  {
    if(clientSock->Connect (InetSocketAddress (servAddress, servPort)) != -1)
    {
      connected = true;
      clientSock->SetSendCallback (MakeCallback(&SendData));
      clientSock->SetRecvCallback (MakeCallback(&HandleRead));
      SendData(clientSock, 0);
    }
  }
  Simulator::Schedule(Seconds(0.1), &dynamicClient, node,
                      clientSock, servAddress, servPort, serverApps);
}

int
main (int argc, char *argv[])
{

  bool tracing = true;
  uint32_t maxBytes = (15*1024);

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

  Ptr<Socket> clientSock = Socket::CreateSocket (nodes.Get(0), 
    TypeId::LookupByName ("ns3::TcpSocketFactory"));

  clientSock->Bind();

  Ipv4Address dstaddr ("10.1.1.2");
//
// Create a TcpServerApplicationApplication and install it on node 1
//
  TcpServerApplicationHelper server (InetSocketAddress (Ipv4Address::GetAny (), port));
  server.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
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

  Simulator::Schedule(Seconds(0.1), &dynamicClient, nodes.Get (0),
                      clientSock, dstaddr, port, serverApps);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  Ptr<TcpServerApplication> sink1 = DynamicCast<TcpServerApplication> (serverApps.Get (0));
  std::cout << "Server Total Bytes Received: " << sink1->GetTotalRx () << std::endl;

  std::cout << "Client Total Bytes Received: " << c_totalRx << std::endl;
}
