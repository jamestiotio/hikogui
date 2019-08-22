// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#include "required.hpp"
#include "atomic.hpp"
#include <array>
#include <atomic>
#include <memory>
#include <thread>
#include <optional>
#include <type_traits>
#include <functional>

namespace TTauri {

template<typename T, int64_t N>
class wfree_mpsc_message_queue;

template<typename T, int64_t N, bool WriteOperation>
class wfree_mpsc_message_queue_operation {
    wfree_mpsc_message_queue<T,N> *parent;
    int64_t index;

public:
    wfree_mpsc_message_queue_operation() noexcept : parent(nullptr), index(0) {}
    wfree_mpsc_message_queue_operation(wfree_mpsc_message_queue<T,N> *parent, int64_t index) noexcept : parent(parent), index(index) {}

    wfree_mpsc_message_queue_operation(wfree_mpsc_message_queue_operation const &other) = delete;

    wfree_mpsc_message_queue_operation(wfree_mpsc_message_queue_operation && other) :
        parent(other.parent), index(other.index)
    {
        if (this == &other) {
            return;
        }
        other.parent = nullptr;
    }

    ~wfree_mpsc_message_queue_operation()
    {
        if (parent == nullptr) {
            return;
        }

        if constexpr (WriteOperation) {
            parent->write_finish(index);
        } else {
            parent->read_finish(index);
        }
    }

    wfree_mpsc_message_queue_operation& operator=(wfree_mpsc_message_queue_operation const &other) = delete;

    wfree_mpsc_message_queue_operation& operator=(wfree_mpsc_message_queue_operation && other)
    {
        if (this == &other) {
            return;
        }

        std::swap(index, other.index);
        std::swap(parent, other.parent);
    }

    T &operator*() noexcept
    {
        required_assert(parent != nullptr);
        return (*parent)[index];
    }
};

template<typename T, int64_t N>
class wfree_mpsc_message_queue {
    using index_type = int64_t;
    using value_type = T;
    using scoped_write_operation = wfree_mpsc_message_queue_operation<T,N,true>;
    using scoped_read_operation = wfree_mpsc_message_queue_operation<T,N,false>;

    enum class message_state { Empty, Copying, Ready };

    struct message_type {
        value_type value;
        std::atomic<message_state> state = message_state::Empty;
    };
    
    static constexpr index_type capacity = N;

    /*! Maximum number of concurent threads that can write into the queue at once.
    */
    static constexpr index_type slack = 16;
    static_assert(capacity > (slack * 2), "The capacity of the message queue should be much larger than its slack.");

    std::array<message_type,capacity> messages;
    std::atomic<index_type> head = 0;
    std::atomic<index_type> tail = 0;

public:
    wfree_mpsc_message_queue() = default;
    wfree_mpsc_message_queue(wfree_mpsc_message_queue const &) = delete;
    wfree_mpsc_message_queue(wfree_mpsc_message_queue &&) = delete;
    wfree_mpsc_message_queue &operator=(wfree_mpsc_message_queue const &) = delete;
    wfree_mpsc_message_queue &operator=(wfree_mpsc_message_queue &&) = delete;
    ~wfree_mpsc_message_queue() = default;

    /*! Return the number of items in the message queue.
    * For the consumer this may show less items in the queue then there realy are.
    */
    typename index_type size() const noexcept {
        // head and tail are extremelly large integers, they will never wrap arround.
        return head.load(std::memory_order_relaxed) - tail.load(std::memory_order_relaxed);
    }

    bool empty() const noexcept {
        return size() == 0;
    }

    bool full() const noexcept {
        return size() >= (capacity - slack);
    }

    /*! Write a message into the queue.
    * This function should only be called when not full().
    * This function is wait-free when the queue is not full().
    *
    * \return A scoped write operation which can be derefenced to access the message value.
    */
    scoped_write_operation write() noexcept {
        return {this, write_start()};
    }

    /*! Read a message from the queue.
    * This function should only be called when not empty().
    * This function will block until the message being read is completed by the writing thread.
    *
    * \return A scoped read operation which can be derefenced to access the message value.
    */
    scoped_read_operation read() noexcept {
        return {this, read_start()};
    }

    value_type const &operator[](index_type index) const noexcept {
        return messages[index % capacity].value;
    }

    value_type &operator[](index_type index) noexcept {
        return messages[index % capacity].value;
    }

    /*! Start a write into the message queue.
     * This function should only be called when not full().
     * This function is wait-free when the queue is not full().
     * Every write_start() must be accompanied by a write_finish().
     *
     * \return The index of the message.
     */
    index_type write_start() noexcept {
        let index = head.fetch_add(1, std::memory_order_acquire);
        auto &message = messages[index % capacity];

        // We acquired the index before we knew if the queue was full.
        // It is assumed that the capacity of the queue is less than the number of threads.
        transition(message.state, message_state::Empty, message_state::Copying, std::memory_order_acquire);
        return index;
    }

    /*! Finish the write of a message.
     * This function is wait-free.
     *
     * \param index The index given from write_start().
     */
    void write_finish(index_type index) noexcept {
        auto &message = messages[index % capacity];
        message.state.store(message_state::Ready, std::memory_order_release);
    }

    /*! Start a read from the message queue.
     * This function should only be called when not empty().
     * This function will block until the message being read is completed by the writing thread.
     * Every read_start() must be accompanied by a read_finish().
     *
     * \return The index of the message.
     */
    index_type read_start() noexcept {
        let index = tail.fetch_add(1, std::memory_order_acquire);
        auto &message = messages[index % capacity];

        // We acquired the index before we knew if the message was ready.
        wait_for_transition(message.state, message_state::Ready, std::memory_order_acquire);
        return index;
    }

    /*! Finish a read from the message queue.
    * This function is wait-free.
    *
    * \param index The index given from read_start().
    */
    void read_finish(index_type index) noexcept {
        auto &message = messages[index % capacity];

        // We acquired the index before we knew if the message was ready.
        message.state.store(message_state::Empty, std::memory_order_release);

        // The message itself does not need to be destructed.
        // This will happen automatically when wrapping around the ring buffer overwrites the message.
    }
};

}