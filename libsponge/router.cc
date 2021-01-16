#include "router.hh"

#include <iostream>
#include <cassert>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

uint64_t Router::gen_subnet_mask(uint8_t length) {
    assert (length <= 32);
    uint64_t mask = static_cast<uint64_t>(1) << length;
    mask -= 1;
    mask = mask << (32 - length);
    return mask;
}

bool Router::matches_route(uint32_t route_prefix, uint8_t prefix_length, uint32_t dst_addr) {
    uint64_t bit_mask = gen_subnet_mask(prefix_length);
    uint64_t route_prefix_u64 = static_cast<uint64_t>(route_prefix);
    route_prefix_u64 = route_prefix_u64 & bit_mask;
    
    uint64_t dst_addr_u64 = static_cast<uint64_t>(dst_addr);
    dst_addr_u64 = dst_addr_u64 & bit_mask;
    return route_prefix_u64 == dst_addr_u64;
}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    // Your code here.
    routes.push_back(std::make_tuple(route_prefix, prefix_length, next_hop, interface_num));
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    // Your code here.
    uint32_t dst_ip = dgram.header().dst;
    uint8_t longest_prefix = static_cast<uint8_t>(0);
    size_t chosen_interface_num = static_cast<size_t>(0);
    size_t default_route_interface = static_cast<size_t>(0);
    bool default_exists = false;
    optional<Address> chosen_next_hop;
    optional<Address> default_next_hop;
    for (const auto& [route_prefix, prefix_length, next_hop, interface_num] : routes) {
        if (matches_route(route_prefix, prefix_length, dst_ip)) {
            if (prefix_length > longest_prefix) {
                longest_prefix = prefix_length;
                chosen_interface_num = interface_num;
                chosen_next_hop = next_hop;
            }
        }
        if (route_prefix == 0 and prefix_length == 0) {
            default_exists = true;
            default_next_hop = next_hop;
            default_route_interface = interface_num;
        }
    }
    if (dgram.header().ttl == 0) {
        return;
    }
    dgram.header().ttl -= 1;
    if (dgram.header().ttl > 0) {
        if (longest_prefix != 0) {
            if (chosen_next_hop.has_value()) {
                interface(chosen_interface_num).send_datagram(dgram, chosen_next_hop.value());
            } else {
                interface(chosen_interface_num).send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
            }
        } else if (default_exists) {
            if (default_next_hop.has_value()) {
                interface(default_route_interface).send_datagram(dgram, default_next_hop.value());
            } else {
                interface(default_route_interface).send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
            }
        }
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
