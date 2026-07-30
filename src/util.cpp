#include "util.h"
#include <iostream>
#include <boost/asio.hpp>

void append_int_vec_char_le(std::vector<char> &buffer, int value)
{
    buffer.push_back(static_cast<char>(value & 0xFF));         // LSB (byte 0)
    buffer.push_back(static_cast<char>((value >> 8) & 0xFF));  // byte 1
    buffer.push_back(static_cast<char>((value >> 16) & 0xFF)); // byte 2
    buffer.push_back(static_cast<char>((value >> 24) & 0xFF)); // MSB (byte 3)
}

void append_prot_packet(std::vector<char> &buffer, int type, size_t len)
{
    buffer.insert(buffer.end(), {'p', 'r', 'o', 't'});
    append_int_vec_char_le(buffer, type);
    append_int_vec_char_le(buffer, static_cast<int>(len));
}

void print_boost_system_error(const char *messageTitle, const boost::system::error_code &ec)
{
    std::cout << "=== BOOST ERROR ===\n";
    std::cout << messageTitle << std::endl;

    if (ec == boost::asio::error::operation_aborted)
    {
        std::cout << "Operation was cancelled(operation_aborted).\n";
    }
    else if (ec == boost::asio::error::eof)
    {
        std::cout << "Connection closed cleanly by peer(eof).\n";
    }
    else
    {
        std::cout << std::format("Other error: {}. what: {} .\n", ec.value(), ec.message());
    }

    std::cout << "===================\n";
}

size_t utf8_length(const std::string_view strview)
{
     size_t len = 0;
    for (size_t i = 0; i < strview.size(); ) {
        unsigned char c = strview[i];
        size_t char_len = 0;

        if ((c & 0x80) == 0) char_len = 1;          // 0xxxxxxx
        else if ((c & 0xE0) == 0xC0) char_len = 2;  // 110xxxxx
        else if ((c & 0xF0) == 0xE0) char_len = 3;  // 1110xxxx
        else if ((c & 0xF8) == 0xF0) char_len = 4;  // 11110xxx
        else throw std::runtime_error("Invalid UTF-8");

        i += char_len;
        ++len;
    }
    return len;
}