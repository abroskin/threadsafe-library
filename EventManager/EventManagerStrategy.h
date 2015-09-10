#ifndef _EVENTMANAGERSTRATEGY_H_
#define _EVENTMANAGERSTRATEGY_H_

#include "EventManager.h"
#include "EventExecutor.h"

class EventManagerStrategy
{
public:
    EventManagerStrategy( EventManager* event_manager )
        : m_event_manager( event_manager )
    {
    }
    virtual ~EventManagerStrategy( )
    {
    }

    virtual void
    send_event( EventManager::CallPair& call, const EventManager::CallType call_type ) = 0;
    virtual void events_post_processing( ) = 0;

protected:
    EventManager* m_event_manager;
};

class SingleThreadEventManagerStrategy : public EventManagerStrategy
{
public:
    SingleThreadEventManagerStrategy( EventManager* event_manager )
        : EventManagerStrategy( event_manager )
    {
    }

    virtual void send_event( EventManager::CallPair& call, const EventManager::CallType );
    virtual void events_post_processing( )
    {
    }
};

class MultipleThreadEventManagerStrategy : public EventManagerStrategy
{
public:
    MultipleThreadEventManagerStrategy( EventManager* event_manager,
                                        const size_t extra_threads_count );
    virtual ~MultipleThreadEventManagerStrategy( );

    virtual void send_event( EventManager::CallPair& call, const EventManager::CallType call_type );
    virtual void events_post_processing( );

private:
    const size_t m_extra_threads_count;
    std::list< EventExecutor > m_executors;

    std::list< EventManager::CallPair > m_main_thread_calls;
    std::list< EventManager::CallPair > m_any_threads_calls;
};

#endif // _EVENTMANAGERSTRATEGY_H_
