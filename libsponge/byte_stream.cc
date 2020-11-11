#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity): cap{capacity}, _error{false} {}

size_t ByteStream::write(const string &data) {
    auto remaining_cap = remaining_capacity();
    std::cout << "Writing data: " << data << std::endl;
    std::cout << "Remaining cap: " << remaining_cap << std::endl;
    if (remaining_cap >= data.size()) {
        buffer += data;
        std::cout << "In buffer: " << buffer << std::endl;
        num_written += data.size();
        used += data.size();
        return data.size();
    } else {
        auto to_write = remaining_cap;
        buffer += data.substr(0, to_write);
        std::cout << "In buffer: " << buffer << std::endl;
        num_written += to_write;
        used += to_write;
        return to_write;
    }
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t to_peek;
    if (used >= len) {
        to_peek = len;
    } else {
        to_peek = used;
    }
    return buffer.substr(0, to_peek);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t to_pop;
    if (used >= len) {
        to_pop = len;
    } else {
        to_pop = used;
    }
    used -= to_pop;
    num_read += to_pop;
    buffer = buffer.substr(to_pop, string::npos);
    std::cout << "In buffer: " << buffer << std::endl;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    auto output = peek_output(len);
    pop_output(len);
    return output;
}

void ByteStream::end_input() { reached_input_end = true;}

bool ByteStream::input_ended() const { return reached_input_end; }

size_t ByteStream::buffer_size() const { return used; }

bool ByteStream::buffer_empty() const { return used == 0; }

bool ByteStream::eof() const { return reached_input_end && used == 0; }

size_t ByteStream::bytes_written() const { return num_written; }

size_t ByteStream::bytes_read() const { return num_read; }

size_t ByteStream::remaining_capacity() const { return cap - used; }
