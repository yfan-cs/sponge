#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : isn_(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , ackno_(isn_)
    , initial_retransmission_timeout_{retx_timeout}
    , stream_(capacity) 
    , timer_(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
    return next_seqno_ - unwrap(ackno_, isn_, next_seqno_); 
}

void TCPSender::fill_window() {
    // absolute upper sequence number:
    if (fin_sent_) return;
    auto window_size = (window_size_ == 0) ? 1 : window_size_;
    auto upper_seqno = unwrap(ackno_, isn_, next_seqno_) + window_size;
    while (next_seqno_ < upper_seqno) {
        auto num_bytes = min(upper_seqno - next_seqno_, TCPConfig::MAX_PAYLOAD_SIZE); 
	bool check_fin = false;
	if (num_bytes == upper_seqno - next_seqno_) {
	    check_fin = true;
	    num_bytes -= (next_seqno_ == 0) ? 1 : 0;
	}
        string &&str = stream_.read(num_bytes);
	// now, construct the TCP Header and payload, and then TCPSegment
	// TCP Header:
	TCPHeader header;
	header.seqno = wrap(next_seqno_, isn_);
	if (next_seqno_ == 0) header.syn = true;
	if (stream_.eof()) {
	    if ( !check_fin || num_bytes > str.size())
	        header.fin = true;
	}
        // payload:
	Buffer payload(move(str));
        // TCPSegment:
	TCPSegment segment;
        segment.header() = header;
        segment.payload() = payload;	
	if (segment.length_in_sequence_space() > 0) {
	    segments_out_.push(segment);
	    segments_outstand_.push(segment);
	    // update next_seqno_
	    next_seqno_ += segment.length_in_sequence_space();
	    timer_.start();
	} else break;
	if (header.fin) {
	    fin_sent_ = true;
	    break;
	}
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto abs_ackno = unwrap(ackno, isn_, next_seqno_);
    if (abs_ackno > next_seqno_) return;
    if (ackno - ackno_ > 0) {
	timer_.set_timeout(initial_retransmission_timeout_);
	timer_.reset_timer();
	if (!segments_outstand_.empty())
	    timer_.start();
	consec_retrans_ = 0;
        ackno_ = ackno;
    }
    
    window_size_ = window_size;
    // step 1: Look through outstanding segments
    while (!segments_outstand_.empty()) {
	auto segment = segments_outstand_.front();
	auto upper_seqno = segment.header().seqno + segment.length_in_sequence_space();
	if (ackno_ - upper_seqno >= 0) {
	    segments_outstand_.pop();
	}
	else break;
    }
    if (segments_outstand_.empty()) timer_.stop();
    // step 2: fill window
    if (window_size > 0 ) {
        fill_window();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    timer_.tick(ms_since_last_tick);
    // check if timer expired
    if (timer_.expired()) {
	// retransmit the earliest oustanding segment
	if (!segments_outstand_.empty())
	    segments_out_.push(segments_outstand_.front());
	// if window_size_ is non-zero
	if (window_size_ != 0 || (unwrap(ackno_, isn_, next_seqno_) == 0)) {
	    ++consec_retrans_;
	    timer_.set_timeout(2 * timer_.get_timeout());
	}
	timer_.reset_timer();
	timer_.start();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return consec_retrans_; }

void TCPSender::send_empty_segment() {
    TCPHeader header;
    header.seqno = wrap(next_seqno_, isn_);
    //if (next_seqno_ == 0) header.syn = true;
    //if (stream_.eof()) header.fin = true;
    TCPSegment segment;
    segment.header() = header;
    segments_out_.push(segment);
}

// ***************************************************************************/
// TCP Timer implementation below:
// ***************************************************************************/

TCPTimer::TCPTimer(const uint16_t retx_timeout)
    : retransmission_timeout_(retx_timeout)
    , time_lapsed_(0)
    , started_(false) {}

void TCPTimer::tick(const size_t ms_since_last_tick) {
    if (started_) time_lapsed_ += ms_since_last_tick;
}

void TCPTimer::set_timeout(const unsigned int timeout) {
    retransmission_timeout_ = timeout;
}

unsigned int TCPTimer::get_timeout() {
    return retransmission_timeout_;
}

bool TCPTimer::expired() {
    return started_ && (time_lapsed_ >= retransmission_timeout_);
}

