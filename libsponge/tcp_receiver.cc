#include "tcp_receiver.hh"

#include <iostream>
#include <stdio.h>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // Set the Initial Sequence Number if necessary
    TCPHeader head = seg.header();
    if (head.syn) {
        isn = head.seqno;
        _syn = true;
        _ack += seg.length_in_sequence_space();
        write2ressembler(seg);
        // std::cout << "the current data is " << std::string(seg.payload().str()) << std::endl;
        // std::cout << "the current _ack is " << wrap(_ack, isn).raw_value() << std::endl;
        if (head.fin) {
            _reassembler.stream_out().end_input();
            return;
        }
    } else {
        // Push any data, or end-of-stream marker, to the StreamReassembler
        if (_syn) {
            write2ressembler(seg);
            // ?
            _ack = _reassembler.stream_out().bytes_written() + 1;
            if (head.fin) {
                _fin = true;
                uint64_t checkpoint = _reassembler.stream_out().bytes_written();
                end_id = unwrap(head.seqno, isn, checkpoint) - 1;
                end_id += seg.payload().str().size();
            }
            // end input
            if (_ack > end_id) {
                _ack++;
                _reassembler.stream_out().end_input();
            }
        }
    }
}

void TCPReceiver::write2ressembler(const TCPSegment &seg) {
    uint64_t checkpoint = _reassembler.stream_out().bytes_written();
    uint64_t index = unwrap(seg.header().seqno, isn, checkpoint);
    index = seg.header().syn ? index : index - 1;
    // std::cout << "the current index is " << index << std::endl;
    // std::cout << "the current string is " << std::string(seg.payload().str()) << std::endl;
    _reassembler.push_substring(seg.payload().copy(), index, _fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn) {
        return {};
    } else {
        return wrap(_ack, isn);
    }
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }