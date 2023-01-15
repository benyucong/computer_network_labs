#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // if received data is greater than windows size, silently discard
    // if (seg.length_in_sequence_space() > window_size()) {
    //     return;
    // }
    // Set the Initial Sequence Number if necessary
    TCPHeader head = seg.header();
    if (head.syn) {
        isn = head.seqno;
        _syn = true;
        if (head.fin) {
            write2ressembler(seg);
            _ack += seg.length_in_sequence_space();
            _reassembler.stream_out().end_input();
            return;
        }
    }
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

            // end input
            optional<WrappingInt32> ack = ackno();
            if (ack.has_value()) {
                WrappingInt32 ack_32 = ack.value();
                uint64_t ack_64 = unwrap(ack_32, isn, checkpoint) - 1;
                printf("%ld\n", ack_64);
                printf("%ld\n", end_id);
                printf("\n");
                if (ack_64 > end_id) {
                    _reassembler.stream_out().end_input();
                }
            } else {
                _reassembler.stream_out().end_input();
            }
        }
    }
}

void TCPReceiver::write2ressembler(const TCPSegment &seg) {
    uint64_t checkpoint = _reassembler.stream_out().bytes_written();
    uint64_t index = unwrap(seg.header().seqno, isn, checkpoint) - 1;
    _reassembler.push_substring(seg.payload().copy(), index, _fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn) {
        return {};
    } else {
        // uint64_t ack = _reassembler.stream_out().bytes_written() + 1;
        // if (_fin)
        //     ++ack;
        return wrap(_ack, isn);
    }
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }