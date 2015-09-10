#include "EventManagerStrategy.h"

void SingleThreadEventManagerStrategy::send_event( EventManager::CallPair& call,
                                                   const EventManager::CallType )
{
    ( *call.first )( call.second );
}

MultipleThreadEventManagerStrategy::MultipleThreadEventManagerStrategy(
    EventManager* event_manager, const size_t extra_threads_count )
    : EventManagerStrategy( event_manager )
    , m_extra_threads_count( extra_threads_count )
{
    m_executors.resize( extra_threads_count );

    for ( std::list< EventExecutor >::iterator it = m_executors.begin( );
          it != m_executors.end( );
          ++it )
    {
        it->start( );
    }
}

MultipleThreadEventManagerStrategy::~MultipleThreadEventManagerStrategy( )
{
    for ( std::list< EventExecutor >::iterator it = m_executors.begin( );
          it != m_executors.end( );
          ++it )
    {
        it->stop( );
    }
}

void MultipleThreadEventManagerStrategy::send_event( EventManager::CallPair& call,
                                                     const EventManager::CallType call_type )
{
    if ( call_type == EventManager::FROM_MAIN_THREAD )
    {
        m_main_thread_calls.push_back( call );
    }
    else
    {
        m_any_threads_calls.push_back( call );
    }
}

void MultipleThreadEventManagerStrategy::events_post_processing( )
{
    std::list< EventManager::CallPair >::iterator main_thread_call = m_main_thread_calls.begin();
    std::list< EventManager::CallPair >::iterator any_threads_call = m_any_threads_calls.begin();

    std::list< EventExecutor >::iterator main_executer = m_executors.begin();
    std::list< EventExecutor >::iterator any_executers_end = m_executors.end();
    std::list< EventExecutor >::iterator any_executers_begin = m_executors.begin();
    if ( m_extra_threads_count > 1u )
    {
        ++any_executers_begin;
    }

    while ( true )
    {
        if ( main_thread_call != m_main_thread_calls.end() )
        {
            if ( main_executer->try_push_for_call( &*main_thread_call ) )
            {
                ++main_thread_call;
            }
        }


        for ( std::list< EventExecutor >::iterator any_executer = any_executers_begin;
              any_executer != any_executers_end;
              ++any_executer )
        {
            if ( any_threads_call == m_any_threads_calls.end() )
            {
                break;
            }

            if ( any_executer->try_push_for_call( &*any_threads_call ) )
            {
                ++any_threads_call;
            }
        }

        if ( main_thread_call == m_main_thread_calls.end() &&
             any_threads_call == m_any_threads_calls.end() )
        {
            break;
        }
    }

    while ( true )
    {
        bool is_in_progress = false;
        for ( std::list< EventExecutor >::iterator executer = m_executors.begin();
              executer != m_executors.end();
              ++executer )
        {
            if ( !executer->is_free() )
            {
                is_in_progress = true;
            }
        }
        if ( !is_in_progress )
        {
            break;
        }
    }

    m_main_thread_calls.clear( );
    m_any_threads_calls.clear( );
}
