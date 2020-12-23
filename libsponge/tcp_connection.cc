#include "tcp_connection.hh"

#include <iostream>
#include <cassert>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { 
    return _sender.stream_in().remaining_capacity(); 
}

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { 
    return current_time - last_segment_received;
}

void TCPConnection::send_segments(bool set_rst) {
    auto& sender_out_queue = _sender.segments_out();
    while (!sender_out_queue.empty()) {
        auto first_seg = sender_out_queue.front();
        if (_receiver.ackno().has_value()) {
            first_seg.header().ackno = _receiver.ackno().value();
        }
        std::numeric_limits<uint8_t> u8_lim;
        size_t win_size = std::min(_receiver.window_size(), static_cast<size_t>(u8_lim.max()));
        first_seg.header().win = static_cast<uint8_t>(win_size);
        first_seg.header().rst = set_rst;
        _segments_out.push(first_seg);
        sender_out_queue.pop();
    }    
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    last_segment_received = current_time;
    auto header = seg.header();
    if (header.rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return;
    }
    _receiver.segment_received(seg);
    if (header.ack) {
        _sender.ack_received(header.ackno, header.win);
    }
    if (seg.length_in_sequence_space() > 0)  {
        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
        assert(_receiver.ackno().has_value());
        send_segments(false);
    }
    if (_receiver.stream_out().input_ended()) {
        if (!_sender.stream_in().eof()) {
            _linger_after_streams_finish = false;
        }
    }
}

bool TCPConnection::active() const { return {}; }

size_t TCPConnection::write(const string &data) {
    auto& sender_stream = _sender.stream_in();
    auto bytes_written = sender_stream.write(data);
    _sender.fill_window();
    send_segments(false);
    return bytes_written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    current_time += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        send_segments(true);
    } 
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
}

void TCPConnection::connect() {
    _sender.fill_window();
    auto syn_segment = _sender.segments_out().front();
    _segments_out.push(syn_segment);
    _sender.segments_out().pop();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            _sender.send_empty_segment();
            send_segments(true);
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
