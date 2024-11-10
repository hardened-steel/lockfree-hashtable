#pragma once
#include <string>
#include <string_view>
#include <random>
#include <map>

template<class Generator>
std::string random_string(std::string_view prefix, std::string::size_type length, Generator& generator)
{
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        " _=+-*/"
    ;
    std::uniform_int_distribution<std::string::size_type> pick(0, sizeof(chars) - 2);

    std::string s;

    s.reserve(length);
    s.append(prefix);
    length -= prefix.size();

    while(length--)
        s += chars[pick(generator)];

    return s;
}

template<class Generator>
std::string random_string(std::string::size_type length, Generator& generator)
{
    return random_string("", length, generator);
}

template<class Generator>
auto generate_random_data(std::size_t size, std::size_t key_size, std::size_t val_size, Generator& generator)
{
    std::vector<std::pair<std::string, std::string>> random_data;
    random_data.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        random_data.emplace_back(random_string(std::to_string(i), key_size, generator), random_string(val_size, generator));
    }
    return random_data;
}
