#include "stream_reassembler.hh"

#include <algorithm>
#include <string>
#include <vector>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

long StreamReassembler::merge(size_t index1, size_t index2, block_node &blk1, block_node &blk2) {
    // guaranted index2 >= index1
    long diff = index2 - index1;
    long merged_len = 0;
    if (static_cast<long>(blk1.length) >= diff) {
        long total_length = blk1.length + blk2.length;
        string merged_string;
        if (blk1.length - diff >= blk2.length) {
            merged_string = blk1.data;
            // blk1's string covers blk2's
            merged_len = blk2.length;
        } else {
            merged_string = blk1.data.substr(0, diff) + blk2.data;
            merged_len = total_length - merged_string.size();
        }
        blk1.data = merged_string;
        blk1.length = blk1.data.size();
        return merged_len;
    } else {
        return -1;
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (index >= _fst_unread + _capacity) {
        return;
    }
    if (index + data.size() <= _fst_unread) {
        set_eof(eof);
    } else if (index < _fst_unread) {
        push_substring(data.substr(_fst_unread - index), _fst_unread, eof);
    } else {
        block_node cur_blk = {data.size(), data};
        _unassembled_bytes += cur_blk.length;

        // merge upstream blks
        auto iter = _buffer.lower_bound(index);
        long merged_bytes = 0;
        while (iter != _buffer.end() && ((merged_bytes = merge(index, iter->first, cur_blk, iter->second)) >= 0)) {
            _unassembled_bytes -= merged_bytes;
            _buffer.erase(iter);
            iter = _buffer.lower_bound(index);
        }

        _buffer[index] = cur_blk;
        iter = _buffer.lower_bound(index);
        // merge downstream blks
        if (iter != _buffer.begin()) {
            --iter;
            merged_bytes = merge(iter->first, index, iter->second, cur_blk);
            if (merged_bytes >= 0) {
                _unassembled_bytes -= merged_bytes;
                _buffer.erase(++iter);
            }
        }
        // write to bytestream
        if (!empty() && _buffer.begin()->first == _fst_unread) {
            size_t write_bytes = _output.write(_buffer.begin()->second.data);
            _fst_unread += write_bytes;
            _unassembled_bytes -= write_bytes;
            _buffer.erase(_buffer.begin());
        }
        set_eof(eof);
    }
}

void StreamReassembler::set_eof(const bool eof) {
    if (eof) {
        _eof = true;
    }
    if (_eof && empty()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }