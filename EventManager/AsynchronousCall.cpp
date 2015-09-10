#include "AsynchronousCall.h"
#include <algorithm>

//namespace
//{

//class AsynCallEvent : public EventManager::Event
//{
//public:
//    AsynCallEvent( EventManager* event_manager, const EventManager::ListenerId listener_id )
//        : EventManager::Event( listener_id )
//        , event_manager( event_manager )
//        , listener_id( listener_id )
//    {
//    }

//    EventManager* event_manager;
//    EventManager::ListenerId listener_id;
//};

//} // namespace

EventManager::ListenerId AsynchronousCall::call( EventManager& event_manager,
                                                 std::function< void( void )> callee,
                                                 EventManager::CallType function_call_type )
{
    // Creating a new listener with a unique event.
    EventManager::Listener listener = std::bind( event_manager_callback_no_ret, _1, callee );

    return add_listener_and_fire_event( event_manager, listener, function_call_type );
}

EventManager::ListenerId
AsynchronousCall::add_listener_and_fire_event( EventManager& event_manager,
                                               EventManager::Listener& listener,
                                               const EventManager::CallType function_call_type )
{
    // Add the listener with the unique event.
    EventManager::ListenerId listener_id = event_manager.add_listener( listener, function_call_type );

    std::auto_ptr< EventManager::Event > event;
    event.reset( new AsynCallEvent( &event_manager, listener_id ) );

    // Fire the new event.
    event_manager.fire_event( event );
    return listener_id;
}

void AsynchronousCall::clean_up_async_call_event( const EventManager::Event* event )
{
    const AsynCallEvent* concrete_event = dynamic_cast< const AsynCallEvent* >( event );
    concrete_event->event_manager->remove_listener( concrete_event->listener_id );
}

void AsynchronousCall::event_manager_callback_no_ret( const EventManager::Event* event,
                                                      std::function< void( void )> callee )
{
    // Call the given function.
    if ( callee )
    {
        callee( );
    }

    clean_up_async_call_event( event );
}
