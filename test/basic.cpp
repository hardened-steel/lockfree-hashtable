#include <memory>
#include <vector>
#include <future>
#include <span>

#include <catch2/catch_all.hpp>
#include <lockfree-hashtable.h>
#include "misc.hpp"

TEST_CASE("create and test hashtable", "[insert][find]") {
    const std::size_t key_size = GENERATE(5, 8, 9, 32, 64);
    const std::size_t val_size = GENERATE(7, 32, 64, 128);
    const lockfree_hashtable_config_t config = {
        10'000,
        key_size,
        val_size
    };

    std::mt19937 generator{std::random_device{}()};
    std::string find;
    find.resize(val_size, ' ');

    std::unique_ptr<std::uint8_t[]> memory(new std::uint8_t[lockfree_hashtable_calc_mem_size(&config)]);

    lockfree_hashtable_t table;
    lockfree_hashtable_init(&table, &config, memory.get());

    SECTION("insert new element") {
        const auto prefix = GENERATE("key1", "key2", "key3");
        const auto key = random_string(prefix, key_size, generator);
        const auto val = random_string("val1", val_size, generator);

        REQUIRE(lockfree_hashtable_insert(&table, key.data(), val.data()));
        REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
        REQUIRE(find == val);

        SECTION("insert additional elements") {
            const auto prefix = GENERATE("key4", "key5", "key6");
            const auto key = random_string(prefix, key_size, generator);
            const auto val = random_string("val2", val_size, generator);

            REQUIRE(lockfree_hashtable_insert(&table, key.data(), val.data()));
            REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
            REQUIRE(find == val);
        }

        REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
        REQUIRE(find == val);
    }

    SECTION("insert elements") {
        const auto prefixes = {"key1", "key2", "key3"};
        for (const auto prefix: prefixes) {
            const auto key = random_string(prefix, key_size, generator);
            const auto val = random_string("val1", val_size, generator);

            REQUIRE(lockfree_hashtable_insert(&table, key.data(), val.data()));
            REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
            REQUIRE(find == val);

            SECTION("insert additional elements") {
                const auto prefixes = {"key3", "key4", "key5"};
                for (const auto prefix: prefixes) {
                    const auto key = random_string(prefix, key_size, generator);
                    const auto val = random_string("val2", val_size, generator);

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

TEST_CASE("fill and check hash table", "[insert][find]") {
    const std::size_t key_size = 64;
    const std::size_t val_size = 128;
    const std::size_t table_size = GENERATE(1, 7, 17, 25, 1000);
    const lockfree_hashtable_config_t config = {
        table_size,
        key_size,
        val_size
    };

    std::mt19937 generator{std::random_device{}()};
    const auto random_data = generate_random_data(table_size, key_size, val_size, generator);
    std::string find;
    find.resize(val_size, ' ');

    std::unique_ptr<std::uint8_t[]> memory(new std::uint8_t[lockfree_hashtable_calc_mem_size(&config)]);

    lockfree_hashtable_t table;
    lockfree_hashtable_init(&table, &config, memory.get());

    SECTION("full table") {
        for (auto& [key, val]: random_data) {
            REQUIRE(lockfree_hashtable_insert(&table, key.data(), val.data()));
            REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
            REQUIRE(find == val);
        }

        for (auto& [key, val]: random_data) {
            REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
            REQUIRE(find == val);
        }

        SECTION("insert addition element") {
            const auto key = random_string("key_1", key_size, generator);
            const auto val = random_string("val1", val_size, generator);

            REQUIRE(!lockfree_hashtable_find(&table, key.data(), nullptr));
            REQUIRE(!lockfree_hashtable_insert(&table, key.data(), val.data()));
        }

        for (auto& [key, val]: random_data) {
            REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
            REQUIRE(find == val);
        }
    }
}

TEST_CASE("concurency fill table", "[insert][find]") {
    const std::size_t key_size = 64;
    const std::size_t val_size = 128;
    const std::size_t table_size = 1'000'000;
    const std::size_t thread_count = GENERATE(2, 3, 7);
    const lockfree_hashtable_config_t config = {
        table_size,
        key_size,
        val_size
    };

    std::mt19937 generator{std::random_device{}()};
    const auto random_data = generate_random_data(table_size * 0.75, key_size, val_size, generator);
    std::string find;
    find.resize(val_size, ' ');

    std::unique_ptr<std::uint8_t[]> memory(new std::uint8_t[lockfree_hashtable_calc_mem_size(&config)]);

    lockfree_hashtable_t table;
    lockfree_hashtable_init(&table, &config, memory.get());

    SECTION("fill table") {
        auto insert = [&] (const std::pair<std::string, std::string>* data, std::size_t size) {
            for (std::size_t i = 0; i < size; ++i) {
                auto& [key, val] = data[i];
                if (!lockfree_hashtable_insert(&table, key.data(), val.data())) {
                    return false;
                }
            }
            return true;
        };
        auto erase = [&] (const std::pair<std::string, std::string>* data, std::size_t size) {
            for (std::size_t i = 0; i < size; ++i) {
                auto& [key, val] = data[i];
                if (!lockfree_hashtable_erase(&table, key.data())) {
                    return false;
                }
            }
            return true;
        };
        auto do_parallel = [&] (auto& function, const auto& random_data) {
            bool result = true;
            std::vector<std::future<bool>> threads;
            threads.reserve(thread_count);

            const auto* data = random_data.data();
            const auto chunk_size = random_data.size() / thread_count;
            for (std::size_t i = 0; i < thread_count; ++i) {
                threads.emplace_back(std::async(std::launch::async, function, data + i * chunk_size, chunk_size));
            }
            if (thread_count * chunk_size < random_data.size()) {
                result &= function(data + thread_count * chunk_size, random_data.size() - thread_count * chunk_size);
            }
            for (auto& th: threads) {
                result &= th.get();
            }
            return result;
        };

        REQUIRE(do_parallel(insert, random_data));

        for (auto& [key, val]: random_data) {
            REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
            REQUIRE(find == val);
        }

        SECTION("erase all elements") {
            REQUIRE(do_parallel(erase, random_data));

            for (auto& [key, val]: random_data) {
                REQUIRE(!lockfree_hashtable_find(&table, key.data(), nullptr));
            }
        }

        SECTION("erase half data") {
            auto first_part = std::span(random_data.data(), random_data.size() / 2);
            auto second_part = std::span(random_data.data() + random_data.size() / 2, random_data.size() / 2);
            
            REQUIRE(do_parallel(erase, first_part));
            for (auto& [key, val]: first_part) {
                REQUIRE(!lockfree_hashtable_find(&table, key.data(), nullptr));
            }
            for (auto& [key, val]: second_part) {
                REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
                REQUIRE(find == val);
            }

            SECTION("insert half back") {
                REQUIRE(do_parallel(insert, first_part));
                for (auto& [key, val]: random_data) {
                    REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
                    REQUIRE(find == val);
                }
            }

            SECTION("insert half back and erase another half") {
                auto i_result = std::async(std::launch::async, [&] { return do_parallel(insert, first_part); });
                auto e_result = std::async(std::launch::async, [&] { return do_parallel(erase, second_part); });

                REQUIRE(i_result.get());
                REQUIRE(e_result.get());
                for (auto& [key, val]: second_part) {
                    REQUIRE(!lockfree_hashtable_find(&table, key.data(), nullptr));
                }
                for (auto& [key, val]: first_part) {
                    REQUIRE(lockfree_hashtable_find(&table, key.data(), find.data()));
                    REQUIRE(find == val);
                }
            }
        }
    }
}
