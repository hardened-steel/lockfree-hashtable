# lockfree hash table

### This project is part of an assessment task:
In C language, implement a hash table <key=64bytes, node=128bytes>
with flat addressing (this means memory for the entire table, including memory for buckets and nodes, should be allocated in a single, continuous buffer).
A hash table implementation cannot use mutex or spinlocks locking primitives.
The collision resolution scheme must be fast.
Implement a consistency check when adding 100'000'000 random nodes with random keys.
Add multithreading of 32 threads, pre-fill the hash map by 3/4 (load factor) of the occupied memory,
but not less than 1'000'000 nodes, in each thread endlessly add a random node with a random key, check for the presence of the added node and delete.

No requirement for rehashing. Flat addressing means that the memory is preallocated at table creation time.
Check consistency means that all elements are added and calculating number if insertions and find per second.

# About struct of project
The project is divided into two main parts:
- Table Implementation is located in "src" folder.
- Tests and benchmarks are located in "test" folder.

# Table Implementations
The table consists of three main parts:
1) The hash table itself consists of pairs of 32 bit values: version of the indirect index and the index itself.
2) Memory area for keys and values. Accessed by index in the table.
3) Bit table of free/occupied records.
