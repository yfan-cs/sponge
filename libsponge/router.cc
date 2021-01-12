#include "router.hh"

#include <iostream>

using namespace std;

// Implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

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

    routes_.emplace_back(route_prefix, prefix_length, next_hop, interface_num);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    int interface_index = -1;
    uint8_t longest_match = 0;
    for (auto &route : routes_) {
        if (prefix_match(route.route_prefix_, route.prefix_length_, dgram.header().dst)) {
	    if (route.prefix_length_ >= longest_match) {
	        // "==" for the case of 0 prefix_length
		interface_index = route.interface_num_;
	        longest_match = route.prefix_length_;	
	    }
	}
    }
    if (interface_index >= 0) {
        if (dgram.header().ttl == 0 || (--dgram.header().ttl == 0)) return;
	auto next_hop = routes_[interface_index].next_hop_;
	if (next_hop.has_value())
            interface(interface_index).send_datagram(dgram, next_hop.value());
	else
            interface(interface_index).send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
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

bool Router::prefix_match(const uint32_t route_prefix,
		          const uint8_t prefix_length,
			  const uint32_t ipaddr) {
    const uint32_t max = 0xffffffff;
    uint32_t mask = (prefix_length == 0) ? 0 : max << (32 - prefix_length);
    const uint32_t res = ~(route_prefix^ipaddr);
    return (res&mask) == mask; 
}
