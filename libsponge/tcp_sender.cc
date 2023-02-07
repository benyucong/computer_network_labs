#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

Retransmission_timer::Retransmission_timer(unsigned int time) : _initial_rto(time), _rto(time) {}
void Retransmission_timer::start_timer() {
    _on = true;
    _time_passed = 0;
}
void Retransmission_timer::stop_timer() {
    if (_time_passed >= _rto) {
        _on = false;
    }
}
void Retransmission_timer::double_rto() { _rto *= 2; }
void Retransmission_timer::timepassing(unsigned int time) { _time_passed += time; }
bool Retransmission_timer::is_on() { return _on; }
bool Retransmission_timer::is_expired() {
    if (_time_passed >= _rto) {
        return true;
    } else {
        return false;
    }
}
void Retransmission_timer::reset_rto() {
    _time_passed = 0;
    _on = true;
}

void Retransmission_timer::reset() {
    _rto = _initial_rto;
    _time_passed = 0;
    _on = false;
}

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _timer(retx_timeout)
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _flybytes; }

void TCPSender::fill_window() {
    // if there are new bytes to be read and space available in the window
    TCPSegment segment;
    segment.header().syn = _syn;
    segment.header().seqno = wrap(_next_seqno, _isn);
    if (_syn) {
        // if SYN is not acked keep sending SYN
        _syn = false;
        send_tcp_seg(segment);
        return;
    }
    size_t maxLength = _freespace;
    if (_freespace == 0) {
        // Not SYN and _windows_size == 0
        if (_window_size == 0 && _start) {
            if (_segments_outstanding.empty()) {
                maxLength = 1;
            } else {
                return;
            }
        } else {
            return;
        }
    }

    size_t payload_bound = std::min(maxLength, TCPConfig::MAX_PAYLOAD_SIZE);
    size_t len = std::min(_stream.buffer_size(), payload_bound);

    if (!segment.header().syn && len == 0) {
        // std::cout << "next seqno is " << _next_seqno <<std::endl;
        // std::cout << "bytes written + 2 is " << _stream.bytes_written() + 2 <<std::endl;
        // to do
        if (_stream.eof() && _next_seqno == _stream.bytes_written() + 1) {
            segment.header().fin = true;
            send_tcp_seg(segment);
            return;
        } else {
            return;
        }
    }
    // std::cout << "The payload is " << _stream.peek_output(len) << std::endl;
    // std::cout << "The stream is " << _stream.peek_output(_stream.buffer_size()) << std::endl;
    string data = _stream.read(len);
    // std::cout << "payload: " << data << std::endl;
    // Important grammer!
    segment.payload() = Buffer(std::move(data));

    // FIN Problem!
    // Don't add FIN if this would make the segment exceed the receiver's window
    if (_stream.eof() && _next_seqno + len == _stream.bytes_written() + 1) {
        segment.header().fin = true;
        if (_freespace < segment.length_in_sequence_space()) {
            segment.header().fin = false;
        }
    }
    // std::cout << "len: " << segment.length_in_sequence_space() << std::endl;

    std::cout << "maxlength: " << maxLength << "; "
              << "buffer_size: " << _stream.buffer_size() << std::endl;
    if (_freespace >= segment.length_in_sequence_space()) {
        _freespace -= segment.length_in_sequence_space();
    }
    send_tcp_seg(segment);
    if (_freespace) {
        fill_window();
    }
    return;
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t ack = unwrap(ackno, _isn, _next_seqno);
    if (ack > _next_seqno) {
        return;
    }
    // to do?
    _freespace = window_size + ack >= _next_seqno ? window_size + ack - _next_seqno : 0;
    _window_size = window_size;

    // remove outstanding ack;
    for (auto it = _segments_outstanding.begin(); it != _segments_outstanding.end();) {
        if (ack_seg(ack, *it)) {
            it = _segments_outstanding.erase(it);
        } else {
            ++it;
        }
    }
    if (ack > _ackno) {
        _start = true;
        _ackno = ack;
        _flybytes = _next_seqno - _ackno;
        _timer.reset();
        if (!_segments_outstanding.empty()) {
            _timer.start_timer();
        }
        _num_retrans = 0;
    }
    fill_window();
}

bool TCPSender::ack_seg(uint64_t ack, TCPSegment &segment) {
    WrappingInt32 start = segment.header().seqno;
    uint64_t start64 = unwrap(start, _isn, ack);
    uint64_t end64 = start64 + segment.length_in_sequence_space() - 1;
    if (ack > end64)
        return true;
    return false;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.timepassing(ms_since_last_tick);
    _timer.stop_timer();

    if (_timer.is_expired()) {
        if (!_segments_outstanding.empty()) {
            TCPSegment seg = _segments_outstanding.front();
            _segments_out.push(seg);
            if (_window_size || seg.header().syn) {
                ++_num_retrans;
                _timer.double_rto();
            }
        }
        _timer.reset_rto();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _num_retrans; }

void TCPSender::send_empty_segment() {}

void TCPSender::send_tcp_seg(TCPSegment &segment) {
    std::cout << "syn: " << segment.header().syn << std::endl;
    std::cout << "fin: " << segment.header().fin << std::endl;
    std::cout << "payload: " << segment.payload().str() << std::endl;
    _segments_out.push(segment);
    _segments_outstanding.push_back(segment);
    if (segment.length_in_sequence_space() && !_timer.is_on()) {
        _timer.start_timer();
    }
    _next_seqno += segment.length_in_sequence_space();
    _flybytes = _next_seqno - _ackno;
    // std::cout << "_next_seqno = " << _next_seqno << "; "
    //           << "_ackno = " << _ackno << "; " << std::endl;
    // std::cout << "flybytes = " << _flybytes << std::endl;
    // std::cout << "The payload is " << segment.payload().str() << std::endl;
    return;
}