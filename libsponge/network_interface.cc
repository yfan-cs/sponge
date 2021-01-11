#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : ethernet_address_(ethernet_address), ip_address_(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(ethernet_address_) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
//! TODO: modularize the construction of an ethernet frame
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    // If the destination eth addr is known, and is not expired, send it right away
    if (ip_to_eth_.find(next_hop_ip) != ip_to_eth_.end()
	&& time_ - update_time_[next_hop_ip] <= 30000) {
        auto eth_addr = ip_to_eth_[next_hop_ip];
	EthernetHeader header;
	header.type = EthernetHeader::TYPE_IPv4;
	header.dst = eth_addr;
	header.src = ethernet_address_;
	EthernetFrame frame;
	frame.header() = header;
	frame.payload() = dgram.serialize();
	frames_out_.push(frame);
	return;
    }
    // queue the IP datagram
    queued_ip_.push(dgram);
    // if an arp request about the IP addr already sent in the last five seconds,
    // don't send a second request
    if (request_time_.find(next_hop_ip) != request_time_.end()
        && time_ - request_time_[next_hop_ip] <= 5000) return;
    // otherwise, send arp request
    request_time_[next_hop_ip] = time_;
    ARPMessage m;
    m.opcode = ARPMessage::OPCODE_REQUEST;
    m.sender_ethernet_address = ethernet_address_;
    m.sender_ip_address = ip_address_.ipv4_numeric();
    m.target_ip_address = next_hop_ip; 
    EthernetHeader header;
    header.type = EthernetHeader::TYPE_ARP;
    header.dst = ETHERNET_BROADCAST;
    header.src = ethernet_address_;
    EthernetFrame frame;
    frame.header() = header;
    frame.payload() = m.serialize();
    frames_out_.push(frame);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst == ETHERNET_BROADCAST || frame.header().dst == ethernet_address_) {
        if (frame.header().type == EthernetHeader::TYPE_IPv4) {
            InternetDatagram ret;
	    if (ret.parse(frame.payload()) == ParseResult::NoError)
		return ret;
	} else if (frame.header().type == EthernetHeader::TYPE_ARP) {
	    ARPMessage message;
	    if (message.parse(frame.payload()) == ParseResult::NoError) {
		ip_to_eth_[message.sender_ip_address] = message.sender_ethernet_address;
		update_time_[message.sender_ip_address] = time_;
		if (message.opcode == ARPMessage::OPCODE_REQUEST
		    && message.target_ip_address == ip_address_.ipv4_numeric()) {
		    // ARP request, send an appropriate reply
		    EthernetHeader header;
		    header.type = EthernetHeader::TYPE_ARP;
		    header.dst = message.sender_ethernet_address;
		    header.src = ethernet_address_;
		    EthernetFrame frame_out;
		    frame_out.header() = header;
		    ARPMessage m;
                    m.opcode = ARPMessage::OPCODE_REPLY;
                    m.sender_ethernet_address = ethernet_address_;
                    m.sender_ip_address = ip_address_.ipv4_numeric();
		    m.target_ethernet_address = message.sender_ethernet_address; 
                    m.target_ip_address = message.sender_ip_address; 
		    frame_out.payload() = m.serialize();
		    frames_out_.push(frame_out);
		}
	        // send queued internet datagrams
		auto sz = queued_ip_.size();
	        while (sz-- != 0) {
                    auto internet_datagram = queued_ip_.front();
		    queued_ip_.pop();
		    send_datagram(internet_datagram, Address::from_ipv4_numeric(message.sender_ip_address));
	        }
            }
	}
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    time_ += ms_since_last_tick; 
}
