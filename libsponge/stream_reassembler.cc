#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool overlap(const Interval& i1, const Interval& i2) {
    const Interval* left, *right;
    if (i1.start < i2.start) {
        left = &i1;
        right = &i2;
    } else if (i1.start > i2.start) {
        left = &i2;
        right = &i1;
    } else {
        return true;
    }
    if (right->start > left->end + 1) {
        return false;
    }
    return true;
}

Interval merge_interval(const Interval& i1, const Interval& i2) {
    const Interval* left, *right;
    if (i1.start < i2.start) {
        left = &i1;
        right = &i2;
    } else if (i1.start > i2.start) {
        left = &i2;
        right = &i1;
    } else {
        if (i1.end >= i2.end) {
            left = &i1;
            right = &i2;    
        } else if (i1.end < i2.end) {
            left = &i2;
            right = &i1;    
        }
    }
    if (left->contains(*right)) {
        return Interval{left->start, left->end, left->data};
    }
    string new_data = left->data.substr(0, right->start - left->start) + right->data;

    return Interval{left->start, right->end, new_data};
}

bool Interval::contains_byte(size_t index) const {
    return start <= index and end >= index;
}

void Interval::truncate(size_t new_length) {
    this->start = this->end - new_length + 1;
    this->data = this->data.substr(this->start, new_length);
}

bool Interval::contains(const Interval& other) const {
    return this->start <= other.start && this->end >= other.start
           && this->start <= other.end && this->end >= other.end;
}

bool StreamReassembler::next_byte_ready() {
    if (intervals.empty()) {
        return false;
    }
    auto first_interval = intervals.front();
    return first_interval.contains_byte(first_unassembled);
}

void StreamReassembler::add_interval(Interval new_interval) {
    if (intervals.size() == 0) {
        intervals.push_back(new_interval);
        return;
    }
    auto it = intervals.begin();
    // std::list<Interval> merged_list;
    bool inserted = false;
    while (it != intervals.end()) {
        if (overlap(new_interval, *it)) {
            new_interval = merge_interval(new_interval, *it);
            it = intervals.erase(it);
            continue;
        } else if (it->start > new_interval.end) {  // doesn't overlap, and is completely to the right
            intervals.insert(it, new_interval);
            inserted = true;
            break;
        }
        it = std::next(it);  // doesn't overlap and *it is completely to the left
    }
    if (!inserted) {
        intervals.push_back(new_interval);
    }
    return;
}


StreamReassembler::StreamReassembler(const size_t capacity) 
: _output(capacity), _capacity(capacity), intervals(), eof_index() {}


size_t StreamReassembler::calc_max_index() {
    return _capacity - _output.buffer_size() + first_unassembled - 1;
}

std::optional<Interval> StreamReassembler::gen_new_interval(const string& data, const size_t index, const bool eof) {
    auto max_index = calc_max_index();
    if (max_index < first_unassembled) {
        return std::optional<Interval>(); // no capacity left
    }
    Interval new_interval;
    new_interval.start = index;
    if (index + data.size() - 1 > max_index) {
        new_interval.end = max_index;
        new_interval.data = data.substr(0, max_index - index + 1);
    } else {
        new_interval.end = index + data.size() - 1;
        new_interval.data = data;
    }
    if (eof) {
        eof_index = std::optional<size_t>(index + data.size() - 1);
    }
    return std::optional(new_interval);
}

bool StreamReassembler::contains_eof_byte(size_t start, size_t end) const {
    if (!eof_index.has_value()) {
        return false;
    }
    size_t eof_index_val = eof_index.value();
    return start <= eof_index_val and eof_index_val <= end;
}

void StreamReassembler::write_to_byte_stream() {
    if (_output.remaining_capacity() < 1) {
        return;
    }
    auto first_interval = intervals.front();
    auto start_of_to_write = first_unassembled - first_interval.start;
    size_t len_to_write = first_interval.end - first_unassembled + 1;
    size_t end_of_to_write;
    if (_output.remaining_capacity() < len_to_write) {
        auto new_len = len_to_write - _output.remaining_capacity();
        string to_write = first_interval.data.substr(start_of_to_write, _output.remaining_capacity());
        end_of_to_write = start_of_to_write + _output.remaining_capacity() - 1;
        first_unassembled = first_unassembled + _output.remaining_capacity();
        _output.write(to_write);
        first_interval.truncate(new_len);
    } else {
        string to_write = first_interval.data.substr(start_of_to_write, len_to_write);
        _output.write(to_write);
        end_of_to_write = first_interval.end;
        first_unassembled = first_interval.end + 1;
        intervals.pop_front();
    }
    if (contains_eof_byte(start_of_to_write, end_of_to_write)) {
        _output.end_input();
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (_output.eof()) {
        return;
    }
    if (data.size() == 0) {
        if (index == first_unassembled and eof) {
            _output.end_input();
        }
        return;
    }
    if (index + data.size() - 1 < first_unassembled) {
        return;
    }
    auto new_interval_op = gen_new_interval(data, index, eof);
    if (!new_interval_op.has_value()) {
        return;
    }
    auto new_interval = new_interval_op.value();
    add_interval(new_interval);
    if (next_byte_ready()) {
        write_to_byte_stream();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t sum_unassembled_bytes = 0;
    for (auto const& interval : intervals) {
        if (interval.end < first_unassembled) {
            continue;
        }
        sum_unassembled_bytes += interval.end - std::max(interval.start, first_unassembled) + 1;
    }
    return sum_unassembled_bytes;
}

bool StreamReassembler::empty() const { return intervals.empty(); }
