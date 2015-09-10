#ifndef _EVENTMANAGER_H_
#define _EVENTMANAGER_H_

#include <list>
#include <map>

class EventExecutor;
class EventManagerStrategy;

/**
 * @brief The EventManager class is a simple mechanism
 * for broadcasting and receiving events from any threads.
 * (e.i., before EventManager::start( ) and after EventManager::stop( ))
 */
class EventManager : public Thread
{
public:
    typedef uint64_t ListenerId;

public:
    /**
     * @brief The Event is a base class for all custom events could be sent through EventManager.
     *
     * EventManager notifies only listeners which subscribed for this specific event type or
     * listener with the given id.
     */
    class Event
    {
    public:
        Event( const std::string& type );
        Event( const ListenerId listener_id );
        virtual ~Event( );

    private:
        std::string m_type;
        ListenerId m_listener_id;

        enum
        {
            BY_LISTENER_TYPE,
            BY_LISTENER_ID
        } m_search;

        friend class EventManager;
    };

public:
    typedef std::function< void( const Event* )> Listener;

    enum CallType
    {
        FROM_MAIN_THREAD,
        FROM_ANY_THREADS
    };

public:
    EventManager( const size_t extra_threads_count = 0u );
    virtual ~EventManager( );

    /**
     * Subscribes a listener for any events of a event_type type.
     * @param event_type is a string type of event for subscription.
     * @param listener is a callback function which will be called after event fired.
     * The parameter (Event*) is read only, so don't save or delete pointer, copy instance instead.
     * @return a unique id of the subscription.
     */
    ListenerId add_listener( const std::string& event_type,
                             Listener& listener,
                             CallType call_type = FROM_MAIN_THREAD );

    /**
     * Adds listener which can be called later by its ListenerId.
     * @param listener is a callback function which will be called after event fired.
     * @return a unique id of the subscription.
     */
    ListenerId add_listener( Listener& listener, CallType call_type = FROM_MAIN_THREAD );

    /**
     * @brief remove_listener removes a subscription with the ListenerId equal id.
     * @param id is an id of the subscription.
     */
    void remove_listener( const ListenerId& id );

    /**
     * @brief fire_event sends the event to all listeners which subscribed to this event type.
     *
     * It is asynchronous function, so EventManager will send the event only in next loop iteration.
     * Note that some event could be not sent if someone will stop EventManager.
     * EventManager deletes an event instance after it send it to listeners.
     * @param event is an auto pointer to an Event instance or derived classes.
     * Assumed that any callbacks know an exact type of instance and can cast base clase to it.
     * EventManager takes ownership of event, don't use the instance after calling this function.
     * @return true if the event successfully pushed in a queue for later sending,
     * and false if the event can't be sent (for instance EventManager is stopped).
     */
    bool fire_event( std::auto_ptr< Event > event );

protected:
    virtual void run( );
    virtual void do_stop( );
    virtual void init( );

private:
    struct ListenerInfo
    {
        Listener listener;
        CallType call_type;
    };

    typedef std::map< ListenerId, ListenerInfo > Id2ListenerMap;

    typedef std::multimap< std::string, Id2ListenerMap::iterator > Type2ListenersMap;
    typedef std::map< ListenerId, Type2ListenersMap::iterator > Id2TypeListenerMap;

    typedef std::list< std::shared_ptr< Event > > EventsList;

    typedef std::pair< Listener*, Event* > CallPair;

    struct AddedListenerInfo
    {
        ListenerId listener_id;
        ListenerInfo listener_info;
        bool is_type_valid;
        std::string type;
    };

private:
    ListenerId get_free_listener_id( );

    void apply_adding_new_listeners( );
    void apply_removing_requested_listeners( );
    void send_events( const EventsList& events );

private:
    /// This pointer to flag which can be set to true from do_stop() only if
    /// EventManager has been stopped from its thread during the sending callbacks.
    Mutex m_events_mutex;
    EventsList m_events;

    Mutex m_listeners_mutex;

    Id2ListenerMap m_listeners;
    Type2ListenersMap m_types;
    Id2TypeListenerMap m_id2type;

    ListenerId m_id_counter;

    std::vector< ListenerId > m_removed_listeners;
    std::vector< AddedListenerInfo > m_added_listeners;
    bool m_is_stop_requested;

    std::shared_ptr< EventManagerStrategy > m_strategy;

    Signal m_signal;

private:
    friend class EventExecutor;
    friend class EventManagerStrategy;
    friend class SingleThreadEventManagerStrategy;
    friend class MultipleThreadEventManagerStrategy;
};

#endif // _EVENTMANAGER_H_
