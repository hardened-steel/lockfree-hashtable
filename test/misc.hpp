#pragma once
#include <string>
#include <string_view>
#include <random>
#include <span>

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

template<class Generator>
void random_string(std::uint32_t prefix, std::span<std::uint8_t> array, Generator& generator)
{
    std::uniform_int_distribution<std::string::size_type> pick(0, std::numeric_limits<std::uint8_t>::max());

    std::memcpy(array.data(), &prefix, sizeof(prefix));
    const auto length = array.size() - sizeof(prefix);

    for (std::size_t i = 0; i < length; ++i) {
        array[i + sizeof(prefix)] = pick(generator);
    }
}

template<class Generator>
auto generate_random_key_val_data(std::size_t prefix, std::size_t size, std::size_t key_size, std::size_t val_size, Generator& generator)
{
    const auto mem_size = size * (key_size + val_size);
    std::unique_ptr<std::uint8_t[]> memory(new std::uint8_t[mem_size]);

    for (std::size_t i = 0; i < mem_size; i += key_size + val_size) {
        auto* key = &memory[i];
        auto* val = &memory[i + key_size];

        random_string(i + prefix, std::span(key, key_size), generator);
        random_string(i + prefix, std::span(val, val_size), generator);
    }
    return memory;
}

template<class Generator>
auto generate_random_key_val_data(std::uint8_t* memory, std::size_t prefix, std::size_t size, std::size_t key_size, std::size_t val_size, Generator& generator)
{
    const auto mem_size = size * (key_size + val_size);

    for (std::size_t i = 0; i < size; ++i) {
        auto* key = &memory[i * (key_size + val_size)];
        auto* val = &memory[i * (key_size + val_size) + key_size];

        random_string(i + prefix, std::span(key, key_size), generator);
        random_string(i + prefix, std::span(val, val_size), generator);
    }
}
