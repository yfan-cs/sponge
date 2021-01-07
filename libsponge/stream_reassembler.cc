#include "stream_reassembler.hh"
#include <iostream>
#include <utility>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : output_(capacity), capacity_(capacity), unassembled_bytes_(0), begin_(0), first_unass_index_(0), eof_(false), buffer_(capacity, make_pair<bool, char>(false, ' ')) { }

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
//! Since the output stream has the same capacity, the write is always successful.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof && index + data.length() <= output_.bytes_read() + capacity_) eof_ = eof;
    if (index <= first_unass_index_) {
        // may push substring to ByteStream
	string pushed;
	for (auto i = first_unass_index_; i < min(index + data.length(), output_.bytes_read() + capacity_); ++i) {
	    check(begin_);
            if (buffer_[begin_].first) {
                buffer_[begin_].first = false;
		--unassembled_bytes_;
	    }
	    pushed.push_back(data[i - index]);
	    begin_ = (begin_ + 1) % capacity_;
	}
	for (auto j = pushed.length(); j < output_.bytes_read() + capacity_ - first_unass_index_; ++j) {
	    check(begin_);
	    if (buffer_[begin_].first) {
                buffer_[begin_].first = false;
		pushed.push_back(buffer_[begin_].second);
		begin_ = (begin_ + 1) % capacity_;
		--unassembled_bytes_;
	    } else break;
	}
	first_unass_index_ += pushed.length();
	// the following write should always be successful
	output_.write(pushed);
	if (eof_ && empty()) output_.end_input();
    } else {
	size_t j = (begin_ + index - first_unass_index_) % capacity_;
	for (auto i = index; i < min(index + data.length(), output_.bytes_read() + capacity_); ++i, j = (j + 1) % capacity_) {
	    check(j);
            if (!buffer_[j].first) {
		buffer_[j].first = true;
		buffer_[j].second = data[i - index];
		++unassembled_bytes_;
	    }
	}	
    }	    
    
}

size_t StreamReassembler::unassembled_bytes() const { return unassembled_bytes_; }

bool StreamReassembler::empty() const { return unassembled_bytes_ == 0; }

void StreamReassembler::check(size_t index) const {
    if (index >= capacity_)
	throw std::out_of_range("Out of range in StreamReassembler!");
}
