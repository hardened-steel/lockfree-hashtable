#include <memory>
#include <vector>
#include <future>
#include <chrono>
#include <iostream>
#include <iomanip>

#include <lockfree-hashtable.h>
#include <string>
#include <string_view>
#include <random>
#include <span>

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

std::mutex m;

template<class Generator>
auto generate_random_key_val_data(std::uint8_t* memory, std::size_t prefix, std::size_t size, std::size_t key_size, std::size_t val_size, Generator& generator)
{
    const auto mem_size = size * (key_size + val_size);

    for (std::size_t i = 0; i < size; ++i) {
        auto* key = &memory[i * (key_size + val_size)];
        auto* val = &memory[i * (key_size + val_size) + key_size];

        random_string(i + prefix, std::span(key, key_size), generator);
        random_string(i + prefix, std::span(val, val_size), generator);

        if (false) {
            std::lock_guard lk(m);
            std::cout << "generate new data (" << i + prefix << "): ";
            for (std::size_t i = 0; i < key_size; ++i) {
                std::cout << std::hex << std::setfill('0') << std::setw(2) << (unsigned)key[i];
            }
            std::cout << " => ";
            for (std::size_t i = 0; i < val_size; ++i) {
                std::cout << std::hex << std::setfill('0') << std::setw(2) << (unsigned)val[i];
            }
            std::cout << std::endl;
        }
    }
}

int main()
{
    const std::size_t key_size = 64;
    const std::size_t val_size = 128;
    const std::size_t table_size = 150'000'000;
    const std::size_t item_count = 100'000'000;
    const std::size_t thread_count = 16;
    const std::size_t chunk_size = 8192;
    const lockfree_hashtable_config_t config = {
        table_size,
        key_size,
        val_size
    };

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
            if (false) {
                std::lock_guard lk(m);
                //std::cout << "generate_data(" << seed << "): " << (prefix + c * chunk_size) << " of size: " << chunk_size << std::endl;
            }
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
            if (false) {
                std::lock_guard lk(m);
                std::cout << std::setprecision (15) << "insert speed: " << chunk_size / elapsed_seconds << " items per second" << std::endl;
            }
        }
        if (size % chunk_size) {
            if (false) {
                std::lock_guard lk(m);
                //std::cout << "generate_data(" << seed << "): " << (prefix + (size / chunk_size) * chunk_size) << " of size: " << size % chunk_size << std::endl;
            }
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
            if (false) {
                std::lock_guard lk(m);
                std::cout << std::setprecision (15) << "insert speed: " << (size % chunk_size) / elapsed_seconds << " items per second" << std::endl;
            }
        }
        {
            std::lock_guard lk(m);
            std::cout << std::setprecision (15) << "insert speed: " << count / time << " items per second" << std::endl;
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
            if (false) {
                std::lock_guard lk(m);
                std::cout << "generate_data(" << seed << "): " << (prefix + c * chunk_size) << " of size: " << chunk_size << std::endl;
            }
            generate_random_key_val_data(data.get(), prefix + c * chunk_size, chunk_size, key_size, val_size, generator);
            auto start = std::chrono::steady_clock::now();
            for (std::size_t i = 0; i < chunk_size; ++i) {
                auto* key = &data[i * (key_size + val_size)];
                auto* val = &data[i * (key_size + val_size) + key_size];
                if (!lockfree_hashtable_find(&table, key, value.get())) {
                    if (false) {
                        std::lock_guard lk(m);
                        std::cout << "cant find a record" << std::endl;
                    }
                    throw std::runtime_error("error find element");
                }
                if (std::memcmp(val, value.get(), val_size) != 0) {
                    if (false) {
                        std::lock_guard lk(m);
                        std::cout << "values not equeal" << std::endl;
                    }
                    throw std::runtime_error("error compare element");
                }
                count += 1;
            }
            auto finish = std::chrono::steady_clock::now();
            double elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
            time += elapsed_seconds;
            if (false) {
                std::lock_guard lk(m);
                std::cout << std::setprecision (15) << "check speed: " << chunk_size / elapsed_seconds << " items per second" << std::endl;
            }
        }
        if (size % chunk_size) {
            if (false) {
                std::lock_guard lk(m);
                std::cout << "generate_data(" << seed << "): " << (prefix + (size / chunk_size) * chunk_size) << " of size: " << size % chunk_size << std::endl;
            }
            generate_random_key_val_data(data.get(), prefix + (size / chunk_size) * chunk_size, size % chunk_size, key_size, val_size, generator);
            auto start = std::chrono::steady_clock::now();
            for (std::size_t i = 0; i < size % chunk_size; ++i) {
                auto* key = &data[i * (key_size + val_size)];
                auto* val = &data[i * (key_size + val_size) + key_size];
                if (!lockfree_hashtable_find(&table, key, value.get())) {
                    if (false) {
                        std::lock_guard lk(m);
                        std::cout << "cant find a record" << std::endl;
                    }
                    throw std::runtime_error("error find element");
                }
                if (std::memcmp(val, value.get(), val_size) != 0) {
                    if (false) {
                        std::lock_guard lk(m);
                        std::cout << "values not equeal: ";
                        for (std::size_t i = 0; i < val_size; ++i) {
                            std::cout << std::hex << std::setfill('0') << std::setw(2) << (unsigned)val[i];
                        }
                        std::cout << " != ";
                        for (std::size_t i = 0; i < val_size; ++i) {
                            std::cout << std::hex << std::setfill('0') << std::setw(2) << (unsigned)value[i];
                        }
                        std::cout << std::endl;
                    }
                    throw std::runtime_error("error compare element");
                }
                count += 1;
            }
            auto finish = std::chrono::steady_clock::now();
            double elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
            time += elapsed_seconds;
            if (false) {
                std::lock_guard lk(m);
                std::cout << std::setprecision (15) << "check speed: " << (size % chunk_size) / elapsed_seconds << " items per second" << std::endl;
            }
        }
        {
            std::lock_guard lk(m);
            std::cout << std::setprecision (15) << "check speed: " << count / time << " items per second" << std::endl;
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

    return 0;
}
