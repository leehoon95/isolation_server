#pragma once
#include <vector>
#include <boost/system/error_code.hpp>
#include <cassert>

void append_int_vec_char_le(std::vector<char>& buffer, int value);

void append_prot_packet(std::vector<char>& buffer, int type, size_t len);

//void print_boost_system_error(const char* messageTitle, const boost::system::error_code &ec);

size_t utf8_length(const std::string_view strview);

#define ASSERT(condition, message) assert((condition) && (message))