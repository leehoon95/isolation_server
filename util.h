#include <vector>

void append_int_vec_char_le(std::vector<char>& buffer, int value)
{
    buffer.push_back(static_cast<char>(value & 0xFF));         // LSB (byte 0)
    buffer.push_back(static_cast<char>((value >> 8) & 0xFF));  // byte 1
    buffer.push_back(static_cast<char>((value >> 16) & 0xFF)); // byte 2
    buffer.push_back(static_cast<char>((value >> 24) & 0xFF)); // MSB (byte 3)
}