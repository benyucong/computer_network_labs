#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _capacity(capacity), left_space(capacity), write_enabled(true), total_write(0), total_read(0) {}

size_t ByteStream::write(const string &data) {
    if (!write_enabled) {
        return 0;
    }
    size_t len_data2write = data.size() < left_space ? data.size() : left_space;
    for (size_t i = 0; i < len_data2write; ++i) {
        _stream.push_back(data[i]);
    }
    left_space -= len_data2write;
    total_write += len_data2write;
    return len_data2write;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t len2peek = len < _stream.size() ? len : _stream.size();
    string ans = string().assign(_stream.begin(), _stream.begin() + len2peek);
    return ans;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t len_data2pop = len < _stream.size() ? len : _stream.size();
    for (size_t i = 0; i < len_data2pop; ++i) {
        _stream.pop_front();
    }
    left_space += len_data2pop;
    total_read += len_data2pop;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ans = peek_output(len);
    // total_read += ans.size();
    pop_output(len);
    return ans;
}

void ByteStream::end_input() { write_enabled = false; }

bool ByteStream::input_ended() const { return (!write_enabled); }

size_t ByteStream::buffer_size() const { return _stream.size(); }

bool ByteStream::buffer_empty() const {
    // printf("the capacity is %ld\n", _capacity);
    // printf("the left space is %ld\n", left_space);
    return _capacity == left_space;
}

bool ByteStream::eof() const { return (_stream.size() == 0) && input_ended(); }

size_t ByteStream::bytes_written() const { return total_write; }

size_t ByteStream::bytes_read() const { return total_read; }

size_t ByteStream::remaining_capacity() const { return left_space; }
