#include <iostream>
#include <functional>
#include <future>
#include <functional>
#include <vector>

#include "hashmap.h"

using namespace std;

void test( std::function<bool()> test_case, const char* test_name )
{
    cout << "TEST '" << test_name << "': " << ( test_case() ? "SUCCESS" : "FAIL" ) << endl;
}

static const size_t INITIAL_MAP_SIZE = 1u;

bool insert_test_case()
{
    Hashmap<int, int> h(INITIAL_MAP_SIZE);

    bool res = h.insert( 1, 2 );

    if ( res )
    {
        res = !h.insert( 1, 2 );
    }

    if ( res )
    {
        res = h.insert( 2, 3 );
    }

    if ( res )
    {
        res = !h.insert( 1, 3 );
    }

    return res;
}

bool remove_test_case()
{
    Hashmap<string, int> h(INITIAL_MAP_SIZE);
    h.insert( "aaa", 2 );
    h.insert( "bbb", 3 );

    bool res = !h.remove( "ccc" );

    if ( res )
    {
        res = h.remove( "aaa" );
    }

    if ( res )
    {
        res = !h.remove( "aaa" );
    }

    if ( res )
    {
        res = h.size() == 1u;
    }

    return res;
}

bool get_test_case()
{
    Hashmap<int, string> h(INITIAL_MAP_SIZE);
    h.insert( 1, "a" );

    string val;
    bool res = !h.get( 0, val );

    if ( res )
    {
        res = h.get( 1, val );
    }

    if ( res )
    {
        res = val == "a";
    }

    if ( res )
    {
        h.remove( 1 );
        res = !h.get( 1, val );
    }

    return res;
}

typedef bool (*WorkFn)( Hashmap<int, int>*, size_t, float );
const int ITERATIONS_COUNT = 20000;
const int THREADS_COUNT = 50;

bool multithread_test_case( WorkFn work, float add_remove_factor )
{
    Hashmap<int, int> map( INITIAL_MAP_SIZE );

    std::srand(std::time(0));

    std::vector< std::future< bool > > futures;
    futures.reserve( THREADS_COUNT );

    for ( size_t i = 0; i < THREADS_COUNT; ++i )
    {
        futures.push_back( std::async( std::launch::async, work, &map, i, add_remove_factor ) );
    }

    bool res = true;
    for ( auto it = futures.begin(); it != futures.end(); ++it )
    {
        res &= it->get();
    }

    return res;
}


bool rand_work( Hashmap<int, int>* map, const size_t work_number, const float add_remove_factor)
{
    for ( int count = 0; count < ITERATIONS_COUNT; ++count )
    {
        int operation_probability = ( std::rand() % 100 ) * add_remove_factor;
        int key = std::rand();
        if ( operation_probability >= 50 )
        {
            map->insert( key, std::rand() );
        }
        else
        {
            map->remove( key );
        }

        int tmp;
        map->get( key, tmp );

    }
    return true;
}

bool unique_work( Hashmap<int, int>* map, const size_t work_number, const float add_remove_factor )
{
    for ( int count = 0; count < ITERATIONS_COUNT; ++count )
    {
        const int range = 10;
        int key = work_number * range + std::rand() % range;
        int value = std::rand();

        int operation_probability = ( std::rand() % 100 ) * add_remove_factor;
        if ( operation_probability >= 50 )
        {
            bool inserted = map->insert( key, value );

            int tmp;
            if ( !map->get( key, tmp ) )
            {
                return false;
            }
            if ( inserted && tmp != value )
            {
                return false;
            }
        }
        else
        {
            map->remove( key );

            int tmp;
            if ( map->get( key, tmp ) )
            {
                return false;
            }
        }

    }
    return true;
}

int main()
{
    test( insert_test_case, "Insert" );
    test( remove_test_case, "Remove" );
    test( get_test_case, "Get" );

    test( std::bind( multithread_test_case, &rand_work, 2.0f), "Multiple threads. Random keys. More insertions." );
    test( std::bind( multithread_test_case, &rand_work, 0.5f), "Multiple threads. Random keys. More deletions." );
    test( std::bind( multithread_test_case, &rand_work, 1.0f), "Multiple threads. Random keys. Balanced." );

    test( std::bind( multithread_test_case, &unique_work, 2.0f), "Multiple threads. Keys from a unique range. More insertions." );
    test( std::bind( multithread_test_case, &unique_work, 0.5f), "Multiple threads. Keys from a unique range. More deletions." );
    test( std::bind( multithread_test_case, &unique_work, 1.0f), "Multiple threads. Keys from a unique range. Balanced." );

    return 0;
}

