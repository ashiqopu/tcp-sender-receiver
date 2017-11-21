Put the working directory inside **scratch** folder

i.e. ns3/scratch/*tcp-sender-receiver/files*

Run using: ./waf --run tcp-sender-receiver


For first IP change time set:

# To observe TCP Port Reuse

./waf --run="tcp-sender-receiver --firstIPchange=0.3"

# To observe TCP [RST] and no transmission to new IP

./waf --run="tcp-sender-receiver --firstIPchange=0.2"
