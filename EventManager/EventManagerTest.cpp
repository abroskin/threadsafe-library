#include "gmock/gmock.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>

#include "EventManager.h"

namespace
{

using namespace std;

class TestEvent : public EventManager::Event
{
public:
    TestEvent( const string& type )
        : Event( type )
    {
    }
};

class TestThread : public Thread
{
public:
    TestThread( )
    {
    }

    TestThread( EventManager* event_manager,
                const int sending_times,
                const int expected_events,
                const vector< string >& subscribe,
                const vector< string >& send )
        : Thread( )
        , m_expected_events( expected_events )
        , m_received_events( 0 )
        , m_event_manager( event_manager )
    {
        for ( vector< string >::const_iterator it = subscribe.begin( ); it != subscribe.end( );
              ++it )
        {
            EventManager::Listener listener = bind( &TestThread::callback, this, placeholders::_1 );
            m_event_manager->add_listener( *it, listener );
        }

        m_sending_events.reserve( send.size( ) * sending_times );
        for ( int i = 0; i < sending_times; ++i )
        {
            for ( vector< string >::const_iterator it = send.begin( ); it != send.end( ); ++it )
            {
                m_sending_events.push_back( ( *it ) );
            }
        }
    }

    virtual ~TestThread( )
    {
    }

    // Thread functions
    virtual void run( )
    {
        while ( true )
        {
            usleep( 10 );
            LockGuard< Mutex > lock_guard( m_lock );
            if ( !send_events( ) )
            {
                break;
            }
        }
        exit( false );
    }

    virtual void do_stop( )
    {
    }

    virtual void init( )
    {
    }

    // EventManager callback
    void callback( const EventManager::Event* event )
    {
        ASSERT_TRUE( event != NULL );

        const TestEvent* concrete_event = dynamic_cast< const TestEvent* >( event );
        ASSERT_TRUE( concrete_event != NULL );

        LockGuard< Mutex > lock_guard( m_lock );
        ++m_received_events;

        send_events( );
    }

    bool is_all_received( )
    {
        LockGuard< Mutex > lock_guard( m_lock );
        return m_received_events == m_expected_events;
    }

private:
    bool send_events( )
    {
        if ( m_sending_events.empty( ) )
        {
            return false;
        }

        m_event_manager->fire_event(
            auto_ptr< EventManager::Event >( new TestEvent( m_sending_events.back( ) ) ) );
        m_sending_events.pop_back( );

        return true;
    }

private:
    int m_expected_events;
    int m_received_events;

    EventManager* m_event_manager;

    Mutex m_lock;

    vector< string > m_sending_events;
};

const char* EVENTS_TYPE[] = { "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "l", "k", "j",
                              "h", "g", "f", "d", "s", "a", "z", "x", "c", "v", "b", "n", "m" };
const size_t EVENTS_TYPE_SZ = sizeof( EVENTS_TYPE ) / sizeof( *EVENTS_TYPE );
// number of iterations can be decreased with env variable, e.g when tests are run with valgrind
const std::string env_var_name = "HCVD_EM_TEST_SENDING_TIMES";
const int SENDING_TIMES = getenv( env_var_name.c_str( ) ) ? atoi( getenv( env_var_name.c_str( ) ) )
                                                          : 1000;
}

TEST( EventManagerTest, stress_test )
{
    EventManager event_manager;
    vector< TestThread* > threads;

    vector< string > receiving_events;
    receiving_events.insert( receiving_events.begin( ), EVENTS_TYPE, EVENTS_TYPE + EVENTS_TYPE_SZ );

    vector< string > sending_events;

    event_manager.start( );

    while ( !receiving_events.empty( ) )
    {
        int receiving_events_num = static_cast< int >( receiving_events.size( ) );
        int expected_events_number = ( receiving_events_num * ( receiving_events_num - 1 ) )
                                     * SENDING_TIMES / 2;

        TestThread* tr;
        tr = new TestThread( &event_manager,
                             SENDING_TIMES,
                             expected_events_number,
                             receiving_events,
                             sending_events );
        threads.push_back( tr );

        sending_events.push_back( receiving_events.back( ) );
        receiving_events.pop_back( );
    }

    vector< TestThread* >::iterator it;
    for ( it = threads.begin( ); it != threads.end( ); ++it )
    {
        ( *it )->start( );
    }

    while ( true )
    {
        usleep( 10000 );

        for ( it = threads.begin( ); it != threads.end( ); ++it )
        {
            if ( !( *it )->is_all_received( ) )
            {
                break;
            }
        }

        if ( it == threads.end( ) )
        {
            break;
        }
    }

    for ( it = threads.begin( ); it != threads.end( ); ++it )
    {
        ( *it )->stop( );
    }

    event_manager.stop( );

    for ( it = threads.begin( ); it != threads.end( ); ++it )
    {
        EXPECT_TRUE( ( *it )->is_all_received( ) );
        delete ( *it );
    }
}
