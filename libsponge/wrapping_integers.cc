#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t isn_u64 = static_cast<uint64_t>(isn.raw_value());
    uint64_t seq_no_u64 = (n + isn_u64) % (static_cast<uint64_t>(1) << 32);
    return WrappingInt32{static_cast<uint32_t>(seq_no_u64)};
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
    uint64_t n_64 = static_cast<uint64_t>(n.raw_value());
    uint64_t isn_64 = static_cast<uint64_t>(isn.raw_value());
    
    uint64_t u32 = static_cast<uint64_t>(1) << 32;
    
    uint64_t init_diff;
    if (n_64 >= isn_64) {
        init_diff = n_64 - isn_64;
    } else {
        init_diff = u32 - isn_64 + n_64;
    }
    if (checkpoint <= init_diff) {
        return init_diff;
    }
    uint64_t multiplier = (checkpoint - init_diff) / u32;
    uint64_t low_num = init_diff + u32 * multiplier;
    uint64_t high_num = init_diff + u32 * (multiplier + 1);
    
    uint64_t low_diff = checkpoint - low_num, high_diff = high_num - checkpoint;
    return low_diff < high_diff ? low_num : high_num;
}
