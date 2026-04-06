# Containers / Datastructures


| Type               | Description                                                               | Normal | coarse_grained | fine_grained | non_blocking | lock_free |
|:-------------------|:--------------------------------------------------------------------------|:------:|:--------------:|:------------:|:------------:|:---------:|
| chan               | Bounded channel / queue for sending and receiving values between threads. |        |       x        |              |              |           |
| unordered_hash_map | Hash map with bucket-based access.                                        |   x    |                |      x       |              |           |
| stack              | LIFO structure; often simple with one lock or advanced with atomics.      |   x    |       x        |              |              |           |
| array              | Fixed-size sequential container.                                          |        |                |              |              |           |
| resize_array       | Dynamically growing array.                                                |        |                |              |              |           |
| ring_buffer        | Circular buffer with fixed capacity.                                      |        |                |              |              |           |
| resize_ring_buffer | Circular buffer that can grow dynamically.                                |        |                |              |              |           |
| vector             | Dynamic contiguous array container.                                       |        |                |              |              |           |
| list               | Linked structure with node-based insertion and removal.                   |        |                |              |              |           |
| graph              | Node-edge structure with many independent regions.                        |        |                |              |              |           |
| lru_cache          | Cache with lookup + eviction state, often map + linked list.              |        |                |              |              |           |
| skip_list          | Layered ordered structure used for concurrent search/insertion.           |        |                |              |              |           |

- Normal
    - Sequential with no synchronization or thread-safety mechanisms.
    - Very simple, fast and has low CPU overhead, but it is not safe for concurrent access.
- Coarse-Grained
  - A single global lock protects the entire data structure or critical section.
  - Safe and easy to implement, but it can become slow due to lock contention.
- Fine-Grained
    - Multiple independent locks protect smaller, specific parts of the data structure simultaneously.
    - Safe and allows more parallelism, but it is more complex to implement.
- Lock-Less
    - Avoids traditional blocking mutexes, often relying on atomics, spinning, or non-blocking coordination.
    - Can be fast, but it is harder to implement correctly and can have very high CPU overhead if internal, tasks take a long time.
- Lock-Free
    - Uses atomic hardware instructions such as Compare-And-Swap to guarantee system-wide progress without blocking locks.
    - Very fast and scalable, but very complex to implement.

# Algorithms

# Crypto Wrapper and Protocols

# API Servers / Clients

# Utility