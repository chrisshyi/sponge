#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer() {}

uint64_t TCPSender::bytes_in_flight() const { return {}; }

void TCPSender::fill_window() {}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { DUMMY_CODE(ackno, window_size); }

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {}


void RetransTimer::start(size_t rto) {
    started = true;
    time_remaining = rto;
    expired = false;
}

void RetransTimer::stop() {
    started = false;
}

bool RetransTimer::has_started() const { return started; }

size_t RetransTimer::get_time_remaining() const {
    return time_remaining;
}

bool RetransTimer::has_expired() const { return expired; }

void RetransTimer::tick(size_t ms_elapsed) {
    if (ms_elapsed >= time_remaining) {
        expired = true;
        return;
    }
    time_remaining -= ms_elapsed;
}

bool operator< (const TCPSegment& s1, const TCPSegment& s2) {
    uint32_t s1_seqno = s1.header().seqno.raw_value(), s2_seqno = s2.header().seqno.raw_value();
    auto u32_less = std::less<uint32_t>();
    return u32_less(s1_seqno, s2_seqno);
}