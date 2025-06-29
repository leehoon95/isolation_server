#pragma once
#include <vector>
#include <boost/system/error_code.hpp>

void append_int_vec_char_le(std::vector<char>& buffer, int value);

void append_prot_packet(std::vector<char>& buffer, int type, size_t len);

void print_boost_system_error(const char* messageTitle, const boost::system::error_code &ec);