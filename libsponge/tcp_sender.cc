#include "tcp_sender.hh"

#include "tcp_config.hh"

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
void Retransmission_timer::stop_timer() { _on = false; }
void Retransmission_timer::double_rto() { _rto *= 2; }
void Retransmission_timer::timepassing(unsigned int time) { _time_passed += time; }
bool Retransmission_timer::is_on() { return _on; }
bool Retransmission_timer::is_expired() {
    if (_on && _time_passed >= _rto) {
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
    _on = true;
}

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return {}; }

void TCPSender::fill_window() {
    // Initialization
    if (_stream.bytes_read() == 0) {
        _syn = true;
    } else {
        _syn = false;
    }

    // if there are new bytes to be read and space available in the window
    size_t win_length = _window_size == 0 ? 1 : _window_size;
    TCPSegment segment;
    segment.header().syn = _syn;
    segment.header().seqno = wrap(_next_seqno, _isn);

    size_t payload_bound = std::min(win_length, TCPConfig::MAX_PAYLOAD_SIZE);
    size_t len = std::min(_stream.buffer_size(), payload_bound);

    if (_stream.input_ended() && len == _stream.buffer_size()) {
        segment.header().fin = true;

        while (segment.length_in_sequence_space() + len > payload_bound) {
            --len;
            segment.header().fin = false;
        }
    }
    string data = _stream.read(len);
    // Important grammer!
    segment.payload() = Buffer(std::move(data));

    _segments_out.push(segment);
    _segments_outstanding.push_back(segment);
    if (len > 0 && !_timer.is_on()) {
        _timer.start_timer();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t ack = unwrap(ackno, _isn, _next_seqno);
    _window_size = window_size;

    // remove outstanding ack;
    for (auto it = _segments_outstanding.begin(); it != _segments_outstanding.end(); ++it) {
        if (ack_seg(ack, *it)) {
            _segments_outstanding.pop_front();
        } else {
            break;
        }
    }
    if (ack > _next_seqno) {
        _next_seqno = ack;
        fill_window();
    }
}

bool TCPSender::ack_seg(uint64_t ack, TCPSegment &segment) {
    WrappingInt32 start = segment.header().seqno;
    uint64_t start64 = unwrap(start, _isn, ack);
    uint64_t end64 = start64 + segment.payload().str().size();
    if (ack > end64)
        return true;
    return false;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    
    _timer.timepassing(ms_since_last_tick);

    if (_timer.is_on() && _timer.is_expired()) {
        if (!_segments_outstanding.empty()) {
            TCPSegment seg = _segments_outstanding.front();
            _segments_out.push(seg);
            if (_window_size) {
                ++num_retrans;
                _timer.double_rto();
            }
        }
        _timer.reset_rto();
    }
    
}

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {}
