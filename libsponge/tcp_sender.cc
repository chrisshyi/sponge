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
    , _cur_rto{retx_timeout}
    , _stream(capacity)
    , _timer() 
    , _segments_in_flight() {}

bool TCPSender::segment_acked(const TCPSegment& segment, const size_t& abs_ackno) {
    auto header = segment.header();
    if (unwrap(header.seqno, _isn, _next_seqno) + segment.length_in_sequence_space() <= abs_ackno) {
        return true;
    }
    return false;
}

void TCPSender::ack_inflight_segments(const size_t& abs_ackno) {
    while (!_segments_in_flight.empty()) {
        auto first_seg = _segments_in_flight.front();
        if (segment_acked(first_seg, abs_ackno)) {
            _segments_in_flight.pop();
            _num_bytes_in_flight -= first_seg.length_in_sequence_space();
        } else {
            break;
        }
    }
    if (_segments_in_flight.empty()) {
        _timer.stop();
    }
}

uint64_t TCPSender::bytes_in_flight() const {
    return _num_bytes_in_flight;
}

TCPSegment TCPSender::gen_new_segment(size_t send_window) { 
    size_t bytes_to_read = send_window;
    TCPSegment new_segment;
    bool set_syn = false, set_fin = false;
    if (_next_seqno == 0) {
        bytes_to_read -= 1;
        set_syn = true;
    }
    if (_stream.eof()) {
        new_segment.header().syn = set_syn;
        new_segment.payload() = Buffer{};
        if (!fin_sent) {
            new_segment.header().fin = true;
            fin_sent = true;
        }
        new_segment.header().seqno = wrap(_next_seqno, _isn);
        _next_seqno += new_segment.length_in_sequence_space();
        return new_segment;
    }
    if (_stream.buffer_size() < bytes_to_read) {
        bytes_to_read = _stream.buffer_size(); // drain the buffer
        if (_stream.input_ended()) {
            set_fin = true;
            fin_sent = true;
        }
    }
    new_segment.header().syn = set_syn;
    new_segment.payload() = Buffer{_stream.read(std::min(bytes_to_read, TCPConfig::MAX_PAYLOAD_SIZE))};
    new_segment.header().fin = set_fin;
    new_segment.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += new_segment.length_in_sequence_space();
    return new_segment;
}

size_t TCPSender::send_segment(size_t send_window) {
    auto new_segment = gen_new_segment(send_window);
    auto seqno_len = new_segment.length_in_sequence_space();
    if (seqno_len == 0) {
        return 0;
    }
    _segments_out.push(new_segment);
    _segments_in_flight.push(new_segment);
    _num_bytes_in_flight += new_segment.length_in_sequence_space();        
    if (!_timer.has_started()) {
        _timer.start(_cur_rto);
    }
    return seqno_len;
}

void TCPSender::fill_window() {
    if (fin_sent) {
        return;
    }
    uint64_t rwnd_u64 = static_cast<uint64_t>(_latest_rwnd);
    if (rwnd_u64 == 0 and _num_bytes_in_flight == 0) {
        send_segment(1);
    } else if (_num_bytes_in_flight < rwnd_u64) {
        size_t window_space = rwnd_u64 - _num_bytes_in_flight;
        while (window_space > 0 and !_stream.eof()) {
            size_t send_window;
            if (window_space > TCPConfig::MAX_PAYLOAD_SIZE + 2) {
                send_window = TCPConfig::MAX_PAYLOAD_SIZE + 2;
            } else {
                send_window = window_space;
            }
            window_space -= send_window;
            send_segment(send_window);
        }
        if (_stream.eof() and !fin_sent and window_space > 0) {
            send_segment(1);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto abs_ackno = unwrap(ackno, _isn, _latest_abs_ack);
    if (abs_ackno > _next_seqno) {
        return;
    }
    if (abs_ackno > _latest_abs_ack) {
        _cur_rto = _initial_retransmission_timeout;
        _num_consec_retrans = 0;
        if (!_segments_in_flight.empty()) {
            _timer.start(_cur_rto);
        }
    }
    _latest_abs_ack = std::max(abs_ackno, _latest_abs_ack);
    ack_inflight_segments(abs_ackno);
    _latest_rwnd = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick(ms_since_last_tick);
    if (_timer.has_started() && _timer.has_expired()) {
        auto earliest_seg = _segments_in_flight.front();
        _segments_out.push(earliest_seg);
        if (_latest_rwnd != 0) {
            _num_consec_retrans += 1;
            _cur_rto += _cur_rto;
        }
        _timer.start(_cur_rto);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _num_consec_retrans; }

void TCPSender::send_empty_segment() {
    _segments_out.push(gen_new_segment(0));
}


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

void RetransTimer::tick(const size_t ms_elapsed) {
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