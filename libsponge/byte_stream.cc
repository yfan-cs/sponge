#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : capacity_(capacity), size_(0), buffer_(capacity, ' '), begin_(0), input_ended_(false), bytes_w_(0), bytes_r_(0) {}

size_t ByteStream::write(const string &data) {
    size_t res(0);
    for (auto c: data) {
        if (size_ < capacity_) {
	    buffer_[begin_ + size_ % capacity_] = c;
	    ++size_;
	    ++res;
	    ++bytes_w_;
	} else break;
    }
    return res;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    std::string res;
    for (size_t i = 0; i < len; ++i) {
        if (i + 1 > size_) break;
	res.push_back(buffer_[begin_ + i % capacity_]);
    }
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (len >= size_) {
	bytes_r_ += size_; 
        size_ = 0;
    } else {
        bytes_r_ += len;
	size_ -= len;
	begin_ = (begin_ + len) % capacity_;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string res;
    size_t i = 0;
    while (i++ < len) {
        if (size_ == 0) break;
	res.push_back(buffer_[begin_ + i % capacity_]);
	--size_;
	++bytes_r_;
    }
    begin_ = begin_ + i % capacity_;
    return res;
}

void ByteStream::end_input() { input_ended_ = true; }

bool ByteStream::input_ended() const { return input_ended_; }

size_t ByteStream::buffer_size() const { return size_; }

bool ByteStream::buffer_empty() const { return size_ == 0; }

bool ByteStream::eof() const { return input_ended_ && size_ == 0; }

size_t ByteStream::bytes_written() const { return bytes_w_; }

size_t ByteStream::bytes_read() const { return bytes_r_; }

size_t ByteStream::remaining_capacity() const { return capacity_ - size_; }
