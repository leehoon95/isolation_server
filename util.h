#pragma once
#include <vector>

void append_int_vec_char_le(std::vector<char>& buffer, int value);

void append_prot_packet(std::vector<char>& buffer, int type, size_t len);