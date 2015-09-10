#include "EventManager.h"
#include "EventManagerStrategy.h"


EventManager::Event::Event( const std::string& type )
    : m_type( type )
    , m_search( BY_LISTENER_TYPE )
{
}

EventManager::Event::Event( const EventManager::ListenerId listener_id )
    : m_listener_id( listener_id )
    , m_search( BY_LISTENER_ID )
{
}

EventManager::Event::~Event( )
{
}

EventManager::EventManager( const size_t extra_threads_count )
    : Thread( "EventManager thread" )
{
    switch ( extra_threads_count )
    {
    case 0:
        m_strategy.reset( new SingleThreadEventManagerStrategy( this ) );
        break;
    default:
        m_strategy.reset( new MultipleThreadEventManagerStrategy( this, extra_threads_count ) );
        break;
    }
}

EventManager::~EventManager( )
{
    if ( is_running( ) )
    {
        stop( );
    }
}

EventManager::ListenerId
EventManager::add_listener( const std::string& event_type, Listener& listener, CallType call_type )
{
    LockGuard< Mutex > lock_guard( m_listeners_mutex );

    AddedListenerInfo new_listener_info;
    new_listener_info.listener_id = get_free_listener_id( );
    new_listener_info.listener_info.listener = listener;
    new_listener_info.listener_info.call_type = call_type;
    new_listener_info.is_type_valid = true;
    new_listener_info.type = event_type;

    m_added_listeners.push_back( new_listener_info );

    m_signal.set( );
    return new_listener_info.listener_id;
}

EventManager::ListenerId
EventManager::add_listener( EventManager::Listener& listener, CallType call_type )
{
    LockGuard< Mutex > lock_guard( m_listeners_mutex );

    AddedListenerInfo new_listener_info;
    new_listener_info.listener_id = get_free_listener_id( );
    new_listener_info.listener_info.listener = listener;
    new_listener_info.listener_info.call_type = call_type;
    new_listener_info.is_type_valid = false;

    m_added_listeners.push_back( new_listener_info );

    m_signal.set( );
    return new_listener_info.listener_id;
}

void EventManager::remove_listener( const ListenerId& id )
{
    LockGuard< Mutex > lock_guard( m_listeners_mutex );
    m_removed_listeners.push_back( id );
    m_signal.set( );
}

bool EventManager::fire_event( std::auto_ptr< Event > event )
{
    LockGuard< Mutex > lock_guard( m_events_mutex );

    if ( m_is_stop_requested )
    {
        return false;
    }

    m_events.push_back( std::shared_ptr< Event >( event.release( ) ) );

    m_signal.set( );
    return true;
}

void EventManager::run( )
{
    while ( true )
    {
        m_signal.wait( true );

        EventsList events_tmp;

        { // m_listeners_mutex lock scope begins
            LockGuard< Mutex > listeners_lock_guard( m_listeners_mutex );

            { // m_events_mutex lock scope begins
                LockGuard< Mutex > events_lock_guard( m_events_mutex );
                events_tmp.splice( events_tmp.begin( ), m_events );

                if ( m_is_stop_requested )
                {
                    break;
                }
            } // m_events_mutex lock scope ends

            apply_adding_new_listeners( );
            apply_removing_requested_listeners( );
        } // m_listeners_mutex lock scope ends

        send_events( events_tmp );
    }

    Thread::exit( false );
}

void EventManager::do_stop( )
{
    LockGuard< Mutex > listeners_lock_guard( m_listeners_mutex );
    LockGuard< Mutex > events_lock_guard( m_events_mutex );

    m_is_stop_requested = true;
    m_signal.set( );
}

void EventManager::init( )
{
    LockGuard< Mutex > listeners_lock_guard( m_listeners_mutex );
    LockGuard< Mutex > events_lock_guard( m_events_mutex );

    m_listeners.clear( );
    m_types.clear( );
    m_id2type.clear( );
    m_removed_listeners.clear( );
    m_added_listeners.clear( );
    m_events.clear( );

    m_is_stop_requested = false;

    m_id_counter = 0u;
}

// Must be under m_listeners_mutex lock.
EventManager::ListenerId EventManager::get_free_listener_id( )
{
    while ( m_listeners.find( m_id_counter ) != m_listeners.end( ) )
    {
        ++m_id_counter;
    }
    return m_id_counter++;
}

// Must be under m_listeners_mutex lock and be called only from EventManager thread.
void EventManager::apply_adding_new_listeners( )
{
    Id2ListenerMap::iterator listener_it = m_listeners.begin( );
    Type2ListenersMap::iterator listener_type_it = m_types.begin( );

    std::vector< AddedListenerInfo >::const_iterator it;
    for ( it = m_added_listeners.begin( ); it != m_added_listeners.end( ); ++it )
    {
        listener_it = m_listeners.insert( listener_it,
                                          std::make_pair( it->listener_id, it->listener_info ) );
        if ( it->is_type_valid )
        {
            listener_type_it
                = m_types.insert( listener_type_it, std::make_pair( it->type, listener_it ) );
            m_id2type.insert( std::make_pair( it->listener_id, listener_type_it ) );
        }
    }

    m_added_listeners.clear( );
}

// Must be under m_listeners_mutex lock and be called only from EventManager thread.
void EventManager::apply_removing_requested_listeners( )
{
    std::vector< ListenerId >::const_iterator it;
    for ( it = m_removed_listeners.begin( ); it != m_removed_listeners.end( ); ++it )
    {
        Id2ListenerMap::iterator listener_it = m_listeners.find( *it );
        if ( listener_it != m_listeners.end( ) )
        {
            m_listeners.erase( listener_it );
        }

        Id2TypeListenerMap::iterator type_it = m_id2type.find( *it );
        if ( type_it != m_id2type.end( ) )
        {
            m_types.erase( type_it->second );
            m_id2type.erase( type_it );
        }
    }

    m_removed_listeners.clear( );
}

// Must be called only from EventManager thread.
void EventManager::send_events( const EventsList& events )
{
    for ( EventsList::const_iterator ev_it = events.begin( ); ev_it != events.end( ); ++ev_it )
    {
        Event* event = ev_it->get( );

        if ( event->m_search == Event::BY_LISTENER_TYPE )
        {
            std::pair< Type2ListenersMap::iterator, Type2ListenersMap::iterator > range
                = m_types.equal_range( event->m_type );

            for ( Type2ListenersMap::iterator it = range.first; it != range.second; ++it )
            {

                Id2ListenerMap::iterator& id2listener_it = it->second;
                CallPair call_pair = std::make_pair( &id2listener_it->second.listener, event );
                m_strategy->send_event( call_pair, id2listener_it->second.call_type );
            }
        }
        else // Event::BY_LISTENER_ID
        {
            Id2ListenerMap::iterator listener_it = m_listeners.find( event->m_listener_id );
            if ( listener_it != m_listeners.end( ) )
            {
                CallPair call_pair = std::make_pair( &listener_it->second.listener, event );
                m_strategy->send_event( call_pair, listener_it->second.call_type );
            }
        }
    }

    m_strategy->events_post_processing( );
}
