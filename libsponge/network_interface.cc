#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

void NetworkInterface::send_eth_frame_ip(const InternetDatagram &dgram, const EthernetAddress& eth_addr) {
    EthernetFrame out_frame;
    EthernetHeader& header = out_frame.header();
    header.dst = eth_addr;
    header.src = this->_ethernet_address;
    header.type = EthernetHeader::TYPE_IPv4;
    
    auto& payload = out_frame.payload();
    payload.append(dgram.serialize());
    _frames_out.push(out_frame);
}

void NetworkInterface::send_eth_frame_arp_req(const uint32_t ip_to_query) {
    ARPMessage arp_msg;
    arp_msg.target_ip_address = ip_to_query;
    arp_msg.sender_ethernet_address = this->_ethernet_address;
    arp_msg.sender_ip_address = this->_ip_address.ipv4_numeric();
    arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
    EthernetFrame out_frame;
    EthernetHeader& header = out_frame.header();
    header.dst = ETHERNET_BROADCAST;
    header.src = this->_ethernet_address;
    header.type = EthernetHeader::TYPE_ARP;

    auto& payload = out_frame.payload();
    payload.append(BufferList{arp_msg.serialize()});
    _frames_out.push(out_frame);
}

void NetworkInterface::send_eth_frame_arp_resp(
    const uint32_t requester_ip,
    const EthernetAddress& requester_eth_addr
) {
    ARPMessage arp_msg;
    arp_msg.target_ip_address = requester_ip;
    arp_msg.target_ethernet_address = requester_eth_addr;
    arp_msg.sender_ethernet_address = this->_ethernet_address;
    arp_msg.sender_ip_address = this->_ip_address.ipv4_numeric();
    arp_msg.opcode = ARPMessage::OPCODE_REPLY;
    EthernetFrame out_frame;
    EthernetHeader& header = out_frame.header();
    header.dst = requester_eth_addr;
    header.src = this->_ethernet_address;
    header.type = EthernetHeader::TYPE_ARP;
    
    auto& payload = out_frame.payload();
    payload.append(BufferList{arp_msg.serialize()});
    _frames_out.push(out_frame);
    
}
//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    auto lookup_res = arp_map.find(next_hop_ip);
    if (lookup_res != arp_map.end()) {
        send_eth_frame_ip(dgram, (*lookup_res).second.first);
    } else {
        auto arp_wait_q_lookup = arp_wait_q.find(next_hop_ip);
        if (arp_wait_q_lookup == arp_wait_q.end()) { // not already waiting for an ARP response
            send_eth_frame_arp_req(next_hop_ip);
            size_t zero = 0;
            arp_wait_q.insert(std::make_pair(next_hop_ip, std::make_pair(std::queue<InternetDatagram>(), zero)));
            arp_wait_q_lookup = arp_wait_q.find(next_hop_ip);
        }
        (*arp_wait_q_lookup).second.first.push(dgram);
    }
}

// Update the ARP map, process queued IP datagrams if necessary
void NetworkInterface::update_arp_map(const uint32_t ip, const EthernetAddress& eth_addr) {
    if (arp_map.find(ip) == arp_map.end()) {
        size_t zero = 0;
        arp_map.insert({
            ip,
            std::make_pair(eth_addr, zero)
        });
    }
    auto lookup_res = arp_wait_q.find(ip);
    if (lookup_res != arp_wait_q.end()) {
        auto wait_q = (*lookup_res).second.first;
        while (!wait_q.empty()) {
            auto front_of_q = wait_q.front();
            send_eth_frame_ip(front_of_q, eth_addr);
            wait_q.pop();
        }
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    const EthernetHeader& frame_header = frame.header();
    if (frame_header.dst == ETHERNET_BROADCAST) {
        if (frame_header.type == EthernetHeader::TYPE_ARP) {
            // ARP request
            ARPMessage arp_msg;
            auto parse_res = arp_msg.parse(static_cast<Buffer>(frame.payload()));
            if (parse_res != ParseResult::NoError) {
                return optional<InternetDatagram>();
            }
            if (arp_msg.target_ip_address != this->_ip_address.ipv4_numeric()) {
                return optional<InternetDatagram>();
            }
            send_eth_frame_arp_resp(arp_msg.sender_ip_address, frame_header.src);
            update_arp_map(arp_msg.sender_ip_address, arp_msg.sender_ethernet_address);
        } 
        return optional<InternetDatagram>();
    } else if (frame_header.dst == this->_ethernet_address) {
        if (frame_header.type == EthernetHeader::TYPE_IPv4) {
            InternetDatagram ip_dgram;
            auto parse_res = ip_dgram.parse(static_cast<Buffer>(frame.payload()));
            if (parse_res != ParseResult::NoError) {
                return optional<InternetDatagram>();
            }
            return optional(ip_dgram);
        } else if (frame_header.type == EthernetHeader::TYPE_ARP) {
            // ARP reply
            ARPMessage arp_msg;
            auto parse_res = arp_msg.parse(static_cast<Buffer>(frame.payload()));
            if (parse_res != ParseResult::NoError) {
                return optional<InternetDatagram>();
            }
            update_arp_map(arp_msg.sender_ip_address, arp_msg.sender_ethernet_address);
        }
    }
        return optional<InternetDatagram>();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto it = arp_map.begin(); it != arp_map.end();) {
        auto new_time = it->second.second + ms_since_last_tick;
        arp_map[it->first] = std::make_pair(it->second.first, new_time);
        if (new_time >= 30000) {
            it = arp_map.erase(it);
        } else {
            it = std::next(it);
        }
    }
    for (auto it = arp_wait_q.begin(); it != arp_wait_q.end();) {
        auto new_time = it->second.second + ms_since_last_tick;
        arp_wait_q[it->first] = std::make_pair(it->second.first, new_time);
        if (new_time >= 5000) {
            it = arp_wait_q.erase(it);
        } else {
            it = std::next(it);
        }
    }
} 
