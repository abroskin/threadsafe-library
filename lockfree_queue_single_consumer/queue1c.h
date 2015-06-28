//
// Created by abroskin on 21.06.15.
//

#ifndef LOCKFREE_QUEUE_SINGLE_CONSUMER_QUEUE1C_H
#define LOCKFREE_QUEUE_SINGLE_CONSUMER_QUEUE1C_H

#include <bits/stl_bvector.h>
#include <stddef.h>
#include <atomic>
#include <assert.h>
#include <thread>

template <class T>
class queue1c
{
public:
    queue1c( size_t size = 1u ):
            m_size(size),
            m_head(0u),
            m_tail_start(0u),
            m_tail_end(0),
            m_resizing(false)
    {
        m_data.resize( size );
    }


public:
//    void push( const T& val )
//    {
//        // Wait for a valid head
//        size_t arr_size;
//        size_t tail_ind;
//
//        do {
//            tail_ind = m_tail_end;
//            arr_size = m_size;
//
//            if ( m_head <= tail_ind - arr_size )
//            {
//                assert(m_head >= tail_ind - arr_size);
//
//                bool resize_by_another_thread = m_resizing.test_and_set();
//                if ( !resize_by_another_thread &&
//                     arr_size == m_size &&
//                     m_head <= tail_ind - arr_size )
//                {
//                    increase_size();
//                }
//                continue;
//            }
//
//        } while ( !std::atomic_compare_exchange_weak(&m_tail_end, &tail_ind, tail_ind + 1) );
//
//
//        m_data[tail_ind  % arr_size] = val;
//
//        const size_t new_tail_s = tail_ind + 1u;
//        while(!std::atomic_compare_exchange_weak_explicit(
//                &m_tail_start,
//                &tail_ind,
//                new_tail_s,
//                std::memory_order_release,
//                std::memory_order_relaxed));
//    }

    void push( const T& val )
    {
        size_t tail_ind;
        size_t new_tail_ind;
        size_t size;

        do {
            if (++m_writers <= 0)
            {
                std::this_thread::yield();
                continue;
            }

            tail_ind = m_tail_end;
            new_tail_ind = tail_ind + 1;
            size = m_size;

            if (m_head <= tail_ind - size)  // Need to do resize.
            {
                //assert(m_head >= tail_ind - size);
                bool resize_by_another_thread = m_resizing.test_and_set();
                if ( resize_by_another_thread )
                {
                    --m_writers;
                    std::this_thread::yield();
                }
                else
                {
                    int expected_val = 1;
                    while ( std::atomic_compare_exchange_weak(
                            &m_writers,
                            &expected_val,
                            std::numeric_limits<decltype(expected_val)>::lowest() ) )
                    {
                        // TO DO: not sure if there should be yield.
                        //std::this_thread::yield();
                    }

                    resize();

                    m_resizing.clear();
                    m_writers = 0;

                    // Can be optimized the continuation.
                }
                continue;
            }

        } while ( !std::atomic_compare_exchange_weak(
                 &m_tail_end,
                 &tail_ind,
                 new_tail_ind) );

        m_data[tail_ind  % size] = val;

        while(!std::atomic_compare_exchange_weak(
                &m_tail_start,
                &tail_ind,
                new_tail_ind));
        --m_writers;
    };



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

    void resize()
    {

    }

private:
    std::vector<T> m_data;
    std::atomic<size_t> m_size;
    std::atomic<size_t> m_head;
    std::atomic<size_t> m_tail_start;
    std::atomic<size_t> m_tail_end;

    std::atomic<int> m_writers;

    std::atomic_flag m_resizing;
};

#endif //LOCKFREE_QUEUE_SINGLE_CONSUMER_QUEUE1C_H
