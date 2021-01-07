#include "tcp_receiver.hh"
#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto header = seg.header();
    // step 1: set isn_ if SYN flag is set
    if (header.syn) {
        isn_ = header.seqno;
	isn_set_ = true;
    }
    // step 2: reassembler_.push_substring(...)
    // checkpoint: last reassembled byte = bytes_written
    uint64_t checkpoint = reassembler_.stream_out().bytes_written() + 1;
    uint64_t stream_index;
    auto abs_seqno = unwrap(header.seqno, isn_, checkpoint);
    if (header.syn && abs_seqno == 0)
	stream_index = 0;
    else
	stream_index = abs_seqno - 1;
    bool eof = header.fin;
    auto str = seg.payload().copy();
    reassembler_.push_substring(str, stream_index, eof);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (isn_set_) {
        uint64_t abs_index = reassembler_.stream_out().bytes_written() + 1;
	if (reassembler_.stream_out().input_ended())
	    ++abs_index;
        return wrap(abs_index, isn_);	
    }
    return {}; 
}

size_t TCPReceiver::window_size() const {
    auto first_unacc = reassembler_.stream_out().bytes_read() + capacity_;
    auto first_unass = reassembler_.stream_out().bytes_written();
    return first_unacc - first_unass;
}
