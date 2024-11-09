#include <random>
#include <memory>
#include <string_view>

#include <catch2/catch_all.hpp>
#include <lockfree-hashtable.h>

std::string random_string(std::string_view prefix, std::string::size_type length)
{
    static const char chars[] = "0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        " _=+-*/"
    ;

    std::mt19937 rg{std::random_device{}()};
    std::uniform_int_distribution<std::string::size_type> pick(0, sizeof(chars) - 2);

    std::string s;

    s.reserve(length);
    s.append(prefix);
    length -= prefix.size();

    while(length--)
        s += chars[pick(rg)];

    return s;
}

TEST_CASE( "create and test hashtable", "[insert][find]" ) {
    const std::size_t key_size = GENERATE(5, 8, 9, 32, 64);
    const std::size_t val_size = GENERATE(7, 32, 64, 128);
    const lockfree_hashtable_config_t config = {
        10'000,
        key_size,
        val_size
    };

    std::unique_ptr<std::uint8_t[]> memory(new std::uint8_t[lockfree_hashtable_calc_mem_size(&config)]);

    lockfree_hashtable_t table;
    lockfree_hashtable_init(&table, &config, memory.get());

    SECTION("insert new element") {
        const auto key = random_string(GENERATE("key1", "key2", "key3"), key_size);
        const auto val = random_string("val1", val_size);
        REQUIRE(lockfree_hashtable_insert(&table, key.data(), val.data()));

        std::string find;
        find.resize(val_size, ' ');

        REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
        REQUIRE(find == val);

        SECTION("insert additional elements") {
            const auto key = random_string(GENERATE("key4", "key5", "key6"), key_size);
            const auto val = random_string("val2", val_size);
            REQUIRE(lockfree_hashtable_insert(&table, key.data(), val.data()));

            std::string find;
            find.resize(val_size, ' ');

            REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
            REQUIRE(find == val);
        }

        REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
        REQUIRE(find == val);
    }

    SECTION("insert elements") {
        std::string find;
        find.resize(val_size, ' ');

        const auto prefixes = {"key1", "key2", "key3"};
        for (const auto prefix: prefixes) {
            const auto key = random_string(prefix, key_size);
            const auto val = random_string("val1", val_size);

            REQUIRE(lockfree_hashtable_insert(&table, key.data(), val.data()));

            REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
            REQUIRE(find == val);

            SECTION("insert additional elements") {
                const auto prefixes = {"key3", "key4", "key5"};
                for (const auto prefix: prefixes) {
                    const auto key = random_string(prefix, key_size);
                    const auto val = random_string("val2", val_size);
                    REQUIRE(lockfree_hashtable_insert(&table, key.data(), val.data()));

                    REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
                    REQUIRE(find == val);
                }
            }

            REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
            REQUIRE(find == val);
        }
    }
}
