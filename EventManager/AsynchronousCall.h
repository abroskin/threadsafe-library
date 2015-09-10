#ifndef _ASYNCHRONOUSCALL_H_
#define _ASYNCHRONOUSCALL_H_

#include "EventManager.h"

class AsynCallEvent : public EventManager::Event
{
public:
    AsynCallEvent( EventManager* event_manager, const EventManager::ListenerId listener_id )
        : EventManager::Event( listener_id )
        , event_manager( event_manager )
        , listener_id( listener_id )
    {
    }

    EventManager* event_manager;
    EventManager::ListenerId listener_id;
};

class AsynchronousCall
{
public:
    /**
     * @copydoc AsynchronousCall::call(EventManager&,std::function< ReturnT( void )>,std::function<
     * void( ReturnT ) >)
     * @param result_callback The function which will be called after callee with result value of callee
     * in a parameter. Parameter type of result_callback must be ReturnT.
     */
    template < typename ReturnT >
    static EventManager::ListenerId call( EventManager& event_manager,
                                          std::function< ReturnT( void )> callee,
                                          std::function< void( ReturnT ) > result_callback,
                                          EventManager::CallType function_call_type = EventManager::FROM_MAIN_THREAD,
                                          EventManager::CallType callback_call_type = EventManager::FROM_MAIN_THREAD );

    /**
     * @copydoc AsynchronousCall::call(EventManager&,std::function< void( void )>)
     * Template type ReturnT is a return type of function binded into callee.
     */
    template < typename ReturnT >
    static EventManager::ListenerId call( EventManager& event_manager,
                                          std::function< ReturnT( void )> callee,
                                          EventManager::CallType function_call_type = EventManager::FROM_MAIN_THREAD );


    /**
     * Calls function callee asynchronously through @see EventManager.
     * @param event_manager @see EventManager does asynchronous events dispatching.
     * @param callee The given function which will be called.
     * @return The id of request.
     */
    static EventManager::ListenerId call( EventManager& event_manager,
                                          std::function< void( void )> callee,
                                          EventManager::CallType function_call_type = EventManager::FROM_MAIN_THREAD );

private:
    static EventManager::ListenerId add_listener_and_fire_event( EventManager& event_manager,
                                                                 EventManager::Listener& listener,
                                                                 const EventManager::CallType function_call_type );

    /// Clean up the event manager by unsubscribing the listener.
    static void clean_up_async_call_event( const EventManager::Event* event );

    template < typename ReturnT >
    static void event_manager_callback( const EventManager::Event* event,
                                        std::function< ReturnT( void )> callee,
                                        std::function< void( ReturnT ) > result_callback,
                                        EventManager::CallType function_call_type,
                                        EventManager::CallType callback_call_type );

    static void event_manager_callback_no_ret( const EventManager::Event* event,
                                               std::function< void( void )> callee );

    template< typename ReturnT >
    static void call_callback( std::function< void( ReturnT ) > result_callback, const ReturnT& val );
};

// -------------------------------------------------------------------------------------------------

namespace
{
using namespace std::placeholders;
} // namespace

template < typename ReturnT >
EventManager::ListenerId AsynchronousCall::call( EventManager& event_manager,
                                                 std::function< ReturnT( void )> callee,
                                                 std::function< void( ReturnT ) > result_callback,
                                                 EventManager::CallType function_call_type,
                                                 EventManager::CallType callback_call_type )
{
    // Creating a new listener with a unique event.
    EventManager::Listener listener
        = std::bind( event_manager_callback< ReturnT >, _1, callee, result_callback, function_call_type, callback_call_type );

    return add_listener_and_fire_event( event_manager, listener, function_call_type );
}

// -------------------------------------------------------------------------------------------------

template < typename ReturnT >
EventManager::ListenerId AsynchronousCall::call( EventManager& event_manager,
                                                 std::function< ReturnT( void )> callee,
                                                 EventManager::CallType function_call_type )
{
    std::function< void( ReturnT ) > empty_result_callback;
    return call< ReturnT >( event_manager, callee, empty_result_callback, function_call_type, EventManager::FROM_ANY_THREADS );
}

// -------------------------------------------------------------------------------------------------

template<typename ReturnT>
void AsynchronousCall::call_callback( std::function< void( ReturnT ) > result_callback, const ReturnT& val )
{
    result_callback( val );
}

// -------------------------------------------------------------------------------------------------

template < typename ReturnT >
void AsynchronousCall::event_manager_callback( const EventManager::Event* event,
                                               std::function< ReturnT( void )> callee,
                                               std::function< void( ReturnT ) > result_callback,
                                               EventManager::CallType function_call_type,
                                               EventManager::CallType callback_call_type )
{
    // Call the given function.
    if ( callee )
    {
        ReturnT ret = callee( );
        if ( result_callback )
        {
            if ( function_call_type == callback_call_type )
            {
                result_callback( ret );
            }
            else
            {
                const AsynCallEvent* concrete_event = dynamic_cast< const AsynCallEvent* >( event );
                std::function<void(void)> fn = std::bind( call_callback<ReturnT>, result_callback, ret );
                call( *concrete_event->event_manager, fn, callback_call_type);
            }
        }
    }

    clean_up_async_call_event( event );
}

#endif // _ASYNCHRONOUSCALL_H_
