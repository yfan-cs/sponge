#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return sender_.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return sender_.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return receiver_.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return time_current_ - time_last_received_; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // Step 0: update last received segment time
    time_last_received_ = time_current_;
    // Step 1: check if RST flag is set:
    if (seg.header().rst) {
	rst_handler();
	return;
    }
    // Otherwise, give the segment to the TCPReciver
    receiver_.segment_received(seg);

    // if inbound stream ends before outbound stream has reached EOF, linger... = false
    // if (receiver_.stream_out().input_ended() && (!sender_.stream_in().eof()))
    if (receiver_.stream_out().input_ended() && (!sender_.fin_sent()))
	linger_after_streams_finish_ = false;

    done();

    // If ACK flag is set, tells the TCPSender about ackno and window_size
    if (seg.header().ack) {
        sender_.ack_received(seg.header().ackno, seg.header().win);
	sender_.fill_window();
	send(false);
    }
    // If seg occupies any sequence numbers, make sure at least one seg is sent in reply
    if (seg.length_in_sequence_space() > 0) {
	sender_.fill_window();
	if (!send(false))
            sender_.send_empty_segment();
	send(false);
    }
    done();
}

bool TCPConnection::active() const { return active_; }

size_t TCPConnection::write(const string &data) {
    auto bytes_written = sender_.stream_in().write(data);
    sender_.fill_window();
    send(false);
    done();
    return bytes_written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // Step 0: update the current time
    time_current_ += ms_since_last_tick;
    // Step 1: tell the TCPSender about the passage of time
    sender_.tick(ms_since_last_tick);
    
    // Step 2: if needed: abort the connection, and send a reset segment to the peer
    if (sender_.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
	if (!send(true))
	    sender_.send_empty_segment();
	send(true);
	rst_handler();
	return;
    }    

    // may need to retransmit
    // because tcp sender put un-acked segments in the queue again
    send(false);

    // Step 3: end the connection cleanly if neccessary (Section 5)
    done();
}

// Shut down the outbound byte stream
void TCPConnection::end_input_stream() {
    sender_.stream_in().end_input();
    sender_.fill_window();
    send(false);
    done();
}

void TCPConnection::connect() {
    write("");
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
	    sender_.fill_window();
	    if (!send(true))
	        sender_.send_empty_segment();
	    send(true);
	    rst_handler();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

// ******************************************************* /
// private helper functions /
// ******************************************************* /

bool TCPConnection::send(bool rst) {
    bool sent = false;
    while (!sender_.segments_out().empty()) {
	sent = true;
        auto segment = sender_.segments_out().front();
	sender_.segments_out().pop();
	fill_header(segment.header(), rst);
	segments_out_.push(segment);
    }
    return sent;
}

void TCPConnection::fill_header(TCPHeader& header, bool rst) {
    // fill in neccessary fields in header
    if (receiver_.ackno().has_value()) {
        header.ackno = receiver_.ackno().value();
	header.ack = true;
    }
    // fill in window size
    if (receiver_.window_size() <= numeric_limits<uint16_t>::max())
        header.win = receiver_.window_size();
    else
	header.win = numeric_limits<uint16_t>::max();
    // Set rst
    header.rst = rst;
}

void TCPConnection::rst_handler() {
    receiver_.stream_out().set_error();
    sender_.stream_in().set_error();
    active_ = false;
}

void TCPConnection::done() {
    bool prereq1 = (receiver_.stream_out().input_ended() && receiver_.unassembled_bytes() == 0);
    bool prereq2 = sender_.fin_sent();
    bool prereq3 = (sender_.bytes_in_flight() == 0);
    bool done = prereq1 && prereq2 && prereq3;
    bool timeout = time_since_last_segment_received() >= 10 * cfg_.rt_timeout;
    if (done && ((!linger_after_streams_finish_) || timeout))
        active_ = false;
}
