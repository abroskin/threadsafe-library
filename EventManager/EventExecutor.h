#ifndef _EVENTEXECUTOR_H_
#define _EVENTEXECUTOR_H_

#include "EventManager.h"

class EventExecutor : public Thread
{
public:
    EventExecutor( );
    virtual ~EventExecutor( );

    bool try_push_for_call( EventManager::CallPair* call_pair );
    bool is_free( );

protected:
    virtual void run( );
    virtual void do_stop( );
    virtual void init( );

private:
    AtomicType< EventManager::CallPair* > m_call;
    Signal m_signal;
    bool m_is_stop_requested;
};

#endif // _EVENTEXECUTOR_H_
