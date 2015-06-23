//
// Created by abroskin on 21.06.15.
//

#ifndef LOCKFREE_QUEUE_SINGLE_CONSUMER_QUEUE1C_H
#define LOCKFREE_QUEUE_SINGLE_CONSUMER_QUEUE1C_H

#include <bits/stl_bvector.h>
#include <stddef.h>
#include <atomic>

template <class T>
class queue1c
{
public:
    queue1c( size_t size = 1u ):
            m_head(0u),
            m_tail_start(0u),
            m_tail_end(0),
            m_writer_wait(false)
    {
        m_data.resize( size );
    }


public:
    void push( const T& val )
    {
        const size_t arr_size = m_data.size();
        size_t tail_ind = m_tail_end++;

        m_data[tail_ind  % arr_size] = val;

        const size_t new_tail_s = tail_ind + 1u;
        while(!std::atomic_compare_exchange_weak_explicit(
                &m_tail_start,
                &tail_ind,
                new_tail_s,
                std::memory_order_release,
                std::memory_order_relaxed));
    }

    // Must be called from one thread.
    void get_all( std::vector<T>& out )
    {
        const size_t tail_start = m_tail_start;
        const size_t head = m_head;
        const size_t distance = tail_start - head;

        if ( distance > 0u )
        {
            out.reserve( out.size() + distance );
            for ( size_t ind = head; ind != tail_start; ++ind )
            {
                out.push_back( m_data[ ind % m_data.size() ] );
            }

            m_head = tail_start;
        }
    }

private:
    std::vector<T> m_data;
    std::atomic<size_t> m_head;
    std::atomic<size_t> m_tail_start;
    std::atomic<size_t> m_tail_end;
    std::atomic_flag m_writer_wait;
};

#endif //LOCKFREE_QUEUE_SINGLE_CONSUMER_QUEUE1C_H
