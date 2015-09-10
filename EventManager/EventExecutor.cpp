#include "EventExecutor.h"

EventExecutor::EventExecutor( ):
    Thread( "EventExecutor" ), m_call( NULL )
{
}

EventExecutor::~EventExecutor( )
{
}

bool EventExecutor::try_push_for_call( EventManager::CallPair* call )
{
    bool pushed = m_call.compare_set_value( NULL, call );
    if ( pushed )
    {
        m_signal.set( );
    }
    return pushed;
}

bool EventExecutor::is_free()
{
    return m_call.value() == NULL;
}

void EventExecutor::run( )
{
    while ( true )
    {
        m_signal.wait( true );

        EventManager::CallPair* call = m_call.value( );
        if ( call )
        {
            ( *call->first )( call->second );

            m_call.set_value( NULL );
        }

        if ( m_is_stop_requested )
        {
            break;
        }
    }

    Thread::exit( false );
}

void EventExecutor::do_stop( )
{
    m_is_stop_requested = true;
    m_signal.set( );
}

void EventExecutor::init( )
{
    m_call.set_value( NULL );
    m_is_stop_requested = false;
}
