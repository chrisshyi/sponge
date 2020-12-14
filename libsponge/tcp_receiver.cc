#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader& header = seg.header();
    if (header.syn) {
        isn = std::optional<WrappingInt32>(header.seqno);
        _reassembler.first_unassembled = 1;
    }
    if (!isn.has_value()) {
        return; // no SYN received yet
    }
    uint64_t abs_seqno = unwrap(header.seqno, isn.value(), _reassembler.first_unassembled);
    if (header.syn) {
        abs_seqno += 1;
    }
    _reassembler.push_substring(seg.payload().copy(), abs_seqno, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!isn.has_value()) {
        return std::optional<WrappingInt32>();
    }
    uint64_t u32 = static_cast<uint64_t>(1) << 32;
    uint64_t seqno_64 = (_reassembler.first_unassembled + static_cast<size_t>(isn.value().raw_value())) % u32;
    uint32_t raw_ackno = static_cast<uint32_t>(seqno_64);
    if (_reassembler.stream_out().input_ended()) {
        raw_ackno += 1; // increment to account for FIN seqno
    }
    return std::optional(WrappingInt32(raw_ackno));
}

size_t TCPReceiver::window_size() const {
    const ByteStream& out_stream = _reassembler.stream_out();
    return _capacity - out_stream.buffer_size();
}
