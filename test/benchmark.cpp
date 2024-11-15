#include <memory>
#include <vector>
#include <future>
#include <chrono>
#include <iostream>
#include <iomanip>

#include <lockfree-hashtable.h>
#include "misc.hpp"

int main()
{
    const std::size_t key_size = 64;
    const std::size_t val_size = 128;
    const std::size_t table_size = 1'250'000;
    const std::size_t item_count = 1'000'000;
    const std::size_t thread_count = 16;
    const std::size_t chunk_size = 8192;
    const std::size_t count_interval = 1'000'000;
    const lockfree_hashtable_config_t config = {
        table_size,
        key_size,
        val_size
    };

    std::mutex m;
    std::random_device random;
    std::string find;
    find.resize(val_size, ' ');

    const auto mem_size = lockfree_hashtable_calc_mem_size(&config);
    std::cout << "used memory: " << (mem_size / (1024.0 * 1024.0 * 1024.0)) << " Gb" << std::endl;
    std::unique_ptr<std::uint8_t[]> memory(new std::uint8_t[mem_size]);

    lockfree_hashtable_t table;
    lockfree_hashtable_init(&table, &config, memory.get());

    auto insert = [&] (std::random_device::result_type seed, std::size_t prefix, std::size_t size) {
        std::size_t count = 0;
        double time = 0;
        std::mt19937 generator{seed};
        
        const auto mem_size = chunk_size * (key_size + val_size);
        std::unique_ptr<std::uint8_t[]> data(new std::uint8_t[mem_size]);

        for (std::size_t c = 0; c < size / chunk_size; ++c) {
            generate_random_key_val_data(data.get(), prefix + c * chunk_size, chunk_size, key_size, val_size, generator);
            auto start = std::chrono::steady_clock::now();
            for (std::size_t i = 0; i < chunk_size; ++i) {
                auto* key = &data[i * (key_size + val_size)];
                auto* val = &data[i * (key_size + val_size) + key_size];
                if (!lockfree_hashtable_insert(&table, key, val)) {
                    throw std::runtime_error("error insert element");
                }
                count += 1;
            }
            auto finish = std::chrono::steady_clock::now();
            double elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
            time += elapsed_seconds;
        }
        if (size % chunk_size) {
            generate_random_key_val_data(data.get(), prefix + (size / chunk_size) * chunk_size, size % chunk_size, key_size, val_size, generator);
            auto start = std::chrono::steady_clock::now();
            for (std::size_t i = 0; i < size % chunk_size; ++i) {
                auto* key = &data[i * (key_size + val_size)];
                auto* val = &data[i * (key_size + val_size) + key_size];
                if (!lockfree_hashtable_insert(&table, key, val)) {
                    throw std::runtime_error("error insert element");
                }
                count += 1;
            }
            auto finish = std::chrono::steady_clock::now();
            double elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
            time += elapsed_seconds;
        }
        return count / time;
    };
    auto check = [&] (std::random_device::result_type seed, std::size_t prefix, std::size_t size) {
        std::size_t count = 0;
        double time = 0;
        std::mt19937 generator{seed};
        std::unique_ptr<std::uint8_t[]> value(new std::uint8_t[val_size]);

        const auto mem_size = chunk_size * (key_size + val_size);
        std::unique_ptr<std::uint8_t[]> data(new std::uint8_t[mem_size]);

        for (std::size_t c = 0; c < size / chunk_size; ++c) {
            generate_random_key_val_data(data.get(), prefix + c * chunk_size, chunk_size, key_size, val_size, generator);
            auto start = std::chrono::steady_clock::now();
            for (std::size_t i = 0; i < chunk_size; ++i) {
                auto* key = &data[i * (key_size + val_size)];
                auto* val = &data[i * (key_size + val_size) + key_size];
                if (!lockfree_hashtable_find(&table, key, value.get())) {
                    throw std::runtime_error("error find element");
                }
                if (std::memcmp(val, value.get(), val_size) != 0) {
                    throw std::runtime_error("error compare element");
                }
                count += 1;
            }
            auto finish = std::chrono::steady_clock::now();
            double elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
            time += elapsed_seconds;
        }
        if (size % chunk_size) {
            generate_random_key_val_data(data.get(), prefix + (size / chunk_size) * chunk_size, size % chunk_size, key_size, val_size, generator);
            auto start = std::chrono::steady_clock::now();
            for (std::size_t i = 0; i < size % chunk_size; ++i) {
                auto* key = &data[i * (key_size + val_size)];
                auto* val = &data[i * (key_size + val_size) + key_size];
                if (!lockfree_hashtable_find(&table, key, value.get())) {
                    throw std::runtime_error("error find element");
                }
                if (std::memcmp(val, value.get(), val_size) != 0) {
                    throw std::runtime_error("error compare element");
                }
                count += 1;
            }
            auto finish = std::chrono::steady_clock::now();
            double elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
            time += elapsed_seconds;
        }
        return count / time;
    };
    auto do_parallel = [&] (std::random_device::result_type seed, auto& function, std::size_t size) {
        double count = 0;
        std::vector<std::future<double>> threads;
        threads.reserve(thread_count);

        const auto chunk_size = size / thread_count;
        for (std::size_t i = 0; i < thread_count; ++i) {
            threads.emplace_back(std::async(std::launch::async, function, seed, i * chunk_size, chunk_size));
        }
        if (thread_count * chunk_size < size) {
            count += function(seed, thread_count * chunk_size, size - thread_count * chunk_size);
        }
        for (auto& th: threads) {
            count += th.get();
        }
        return count / (thread_count + (thread_count * chunk_size < size ? 1 : 0));
    };

    const auto seed = random();
    {
        auto start = std::chrono::steady_clock::now();

        auto result = do_parallel(seed, insert, item_count);

        auto finish = std::chrono::steady_clock::now();
        double elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
        std::cout << std::setprecision (15) << "insert(" << result << ") estimated: " << elapsed_seconds << " seconds" << std::endl;
    }
    {
        auto start = std::chrono::steady_clock::now();

        auto result = do_parallel(seed, check, item_count);

        auto finish = std::chrono::steady_clock::now();
        double elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
        std::cout << std::setprecision (15) << "check (" << result << ") estimated: " << elapsed_seconds << " seconds" << std::endl;
    }
    {
        auto insert_and_check = [&] (std::random_device::result_type seed, std::size_t prefix) {
            std::size_t count  = 0;
            double insert_time = 0;
            double find_time   = 0;
            std::unique_ptr<std::uint8_t[]> data(new std::uint8_t[key_size + val_size]);
            std::unique_ptr<std::uint8_t[]> value(new std::uint8_t[val_size]);
            std::mt19937 generator{seed};
            generate_random_key_val_data(data.get(), prefix, 1, key_size, val_size, generator);
            for (;; count = 0) {
                for (; count < count_interval; ++count) {
                    auto* key = &data[0];
                    auto* val = &data[key_size];
                    {
                        auto start = std::chrono::steady_clock::now();
                        if (!lockfree_hashtable_insert(&table, key, val)) {
                            std::lock_guard lk(m);
                            std::cout << "error insert element" << std::endl;
                            throw std::runtime_error("error insert element");
                        }
                        auto finish = std::chrono::steady_clock::now();
                        insert_time += std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
                    }
                    {
                        auto start = std::chrono::steady_clock::now();
                        if (!lockfree_hashtable_find(&table, key, value.get())) {
                            std::lock_guard lk(m);
                            std::cout << "error find element" << std::endl;
                            throw std::runtime_error("error find element");
                        }
                        auto finish = std::chrono::steady_clock::now();
                        find_time += std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
                        if (std::memcmp(val, value.get(), val_size) != 0) {
                            std::lock_guard lk(m);
                            std::cout << "error compare element" << std::endl;
                            throw std::runtime_error("error compare element");
                        }
                    }
                }
                std::lock_guard lk(m);
                std::cout << std::setprecision (15) << "insert speed: " << count / insert_time << " items per second" << std::endl;
                std::cout << std::setprecision (15) << "find speed: " << count / find_time << " items per second" << std::endl;
            }
        };
        std::vector<std::future<void>> threads;
        threads.reserve(thread_count);

        for (std::size_t i = 0; i < thread_count; ++i) {
            threads.emplace_back(std::async(std::launch::async, insert_and_check, random(), i + item_count));
        }
        for (auto& th: threads) {
            th.get();
        }
    }

    return 0;
}
