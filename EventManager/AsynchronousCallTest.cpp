#include "gmock/gmock.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>

#include "EventManager.h"
#include "AsynchronousCall.h"

namespace
{
using namespace std;
using namespace std::placeholders;

Signal test_signal;
int global_val = 0;
bool received_result = false;

void void_void_sig( )
{
    global_val = 1;
    test_signal.set( );
}

bool void_bool_sig( )
{
    global_val = 2;
    test_signal.set( );
    return true;
}

bool void_bool( )
{
    global_val = 3;
    return true;
}

void cb_bool( bool res )
{
    received_result = res;
    test_signal.set( );
}

class AsyncTestClass
{
public:
    double foo( int param )
    {
        foo_param = param;
        return double( param + 1 );
    }
    void bar( double param )
    {
        bar_param = param;
        test_signal.set( );
    }

    int foo_param;
    double bar_param;
};

int64_t total_amount;
int64_t counter;
std::vector< int64_t > counter_history;

} // namespace

class AsynchronousCallTest : public ::testing::Test
{
public:
    AsynchronousCallTest():
        event_manager( 10u )
    {
    }

    virtual void SetUp( )
    {
        global_val = 0;
        received_result = false;

        counter = 0;
        total_amount = 0;
        counter_history.clear( );

        event_manager.start( );
    }

    virtual void TearDown( )
    {
        event_manager.stop( );
    }

    EventManager event_manager;
};

// -------------------------------------------------------------------------------------------------

TEST_F( AsynchronousCallTest, global_void_functions_simple_call )
{
    AsynchronousCall::call( event_manager, &void_void_sig );
    test_signal.wait( true );
    EXPECT_EQ( 1, global_val );
}

TEST_F( AsynchronousCallTest, global_functions_simple_call )
{
    AsynchronousCall::call< bool >( event_manager, &void_bool_sig );
    test_signal.wait( true );
    EXPECT_EQ( 2, global_val );
}

TEST_F( AsynchronousCallTest, global_functions_call_with_return )
{
    AsynchronousCall::call< bool >( event_manager, &void_bool, &cb_bool );
    test_signal.wait( true );
    EXPECT_EQ( 3, global_val );
    EXPECT_TRUE( received_result );
}

TEST_F( AsynchronousCallTest, class_methods_call )
{
    AsyncTestClass obj;
    const int param = 42;
    AsynchronousCall::call< double >( event_manager,
                                      std::bind( &AsyncTestClass::foo, &obj, param ),
                                      std::bind( &AsyncTestClass::bar, &obj, _1 ) );
    test_signal.wait( true );
    EXPECT_EQ( param, obj.foo_param );
    EXPECT_FLOAT_EQ( double( param + 1 ), obj.bar_param );
}

// -------------------------------------------------------------------------------------------------

namespace
{

// number of iterations can be decreased with env variable, e.g when tests are run with valgrind
const std::string env_var_name = "HCVD_AC_TEST_THREADS_NUMBER";
const int THREADS_NUMBER = getenv( env_var_name.c_str( ) ) ? atoi( getenv( env_var_name.c_str( ) ) )
                                                           : 20;

int64_t call_func( uint64_t sleep_time )
{
    int64_t temp = counter;
    counter = 0;
    usleep( sleep_time );
    counter = temp + 1;
    return temp;
}

void callback( int64_t val )
{
    counter_history.push_back( val );
    if ( val + 1 == total_amount )
    {
        test_signal.set( );
    }
}

class TestThread : public Thread
{
public:
    TestThread( EventManager* event_manager, const int64_t calls_count )
        : Thread( )
        , m_calls_count( calls_count )
        , m_event_manager( event_manager )
    {
    }

    virtual ~TestThread( )
    {
    }

    virtual void do_stop( )
    {
    }

    virtual void init( )
    {
    }

    // Thread functions
    virtual void run( )
    {
        for ( int64_t i = 0; i < m_calls_count * m_calls_count * m_calls_count; ++i )
        {
            AsynchronousCall::call< int64_t >(
                *m_event_manager,
                std::bind( call_func, THREADS_NUMBER - m_calls_count ),
                callback );
        }
        exit( false );
    }

    const int64_t m_calls_count;
    EventManager* m_event_manager;
};

} // namespace

TEST_F( AsynchronousCallTest, multiple_threads )
{
    std::shared_ptr< TestThread > threads[THREADS_NUMBER];

    for ( int64_t i = 0; i < THREADS_NUMBER; ++i )
    {
        int64_t curr_count = i * i * i;
        total_amount += curr_count;
        threads[i].reset( new TestThread( &event_manager, i ) );
    }

    for ( int i = THREADS_NUMBER - 1; i >= 0; --i )
    {
        threads[i]->start( );
    }

    ASSERT_TRUE( test_signal.wait( true, 100000u ) );
    usleep( 1000 );

    EXPECT_EQ( total_amount, counter );
    EXPECT_EQ( total_amount, int64_t( counter_history.size( ) ) );
    for ( size_t i = 1; i < counter_history.size( ); ++i )
    {
        EXPECT_EQ( counter_history[i - 1] + 1, counter_history[i] );
    }
}

// -------------------------------------------------------------------------------------------------

namespace
{
    struct SignalSetter
    {
        void operator()( Signal* signal )
        {
            signal->set();
        }
    };

    int long_task()
    {
        usleep(100000u);
        return 1;
    }

    void collect_results( int* final_result, std::shared_ptr<Signal> /*final_signal*/, int long_work_result )
    {
        *final_result += long_work_result;
    }
} // namespace

TEST_F( AsynchronousCallTest, parallel_task_with_synchronous_result )
{
    const int task_count = THREADS_NUMBER * 10;

    int result = 0;
    Signal signal;

    {
        std::shared_ptr<Signal> signal_setter = std::shared_ptr<Signal>( &signal, SignalSetter() );

        for ( int i = 0; i < task_count; ++i )
        {
            AsynchronousCall::call< int >( event_manager,
                                           long_task,
                                           std::bind( &collect_results, &result, signal_setter, _1 ),
                                           EventManager::FROM_ANY_THREADS,
                                           EventManager::FROM_MAIN_THREAD );
        }
    }

    signal.wait( false );
    EXPECT_EQ( task_count, result );
}
