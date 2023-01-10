#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t MODEBASE = 1ul << 32;

    uint64_t seqno64 = (n + isn.raw_value()) % MODEBASE;

    uint32_t seqno32 = static_cast<uint32_t>(seqno64);
    return WrappingInt32{seqno32};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t diff;
    if (n.raw_value() >= isn.raw_value()) {
        diff = n.raw_value() - isn.raw_value();
    } else {
        diff = n.raw_value() + (1ul << 32) - isn.raw_value();
    }
    if (checkpoint <= diff) {
        return diff;
    }
    uint64_t mag = (checkpoint - diff) >> 32;
    uint64_t candidate1 = (mag << 32) + diff;
    uint64_t candidate2 = ((++mag) << 32) + diff;
    if (abs(candidate1, checkpoint) < abs(candidate2, checkpoint)) {
        return candidate1;
    }
    return candidate2;
}

uint64_t abs(uint64_t a, uint64_t b) {
    if (a > b)
        return a - b;
    else
        return b - a;
}