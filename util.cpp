#include "util.h"

void append_int_vec_char_le(std::vector<char>& buffer, int value)
{
    buffer.push_back(static_cast<char>(value & 0xFF));         // LSB (byte 0)
    buffer.push_back(static_cast<char>((value >> 8) & 0xFF));  // byte 1
    buffer.push_back(static_cast<char>((value >> 16) & 0xFF)); // byte 2
    buffer.push_back(static_cast<char>((value >> 24) & 0xFF)); // MSB (byte 3)
}

void append_prot_packet(std::vector<char>& buffer, int type, size_t len)
{
    buffer.insert(buffer.end(), {'p', 'r', 'o', 't'});
    append_int_vec_char_le(buffer, type);
    append_int_vec_char_le(buffer, static_cast<int>(len));
}