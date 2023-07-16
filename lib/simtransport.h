

#ifndef _LIB_SIMTRANSPORT_H_
#define _LIB_SIMTRANSPORT_H_

#include "lib/transport.h"
#include "lib/transportcommon.h"

#include <deque>
#include <map>
#include <functional>

class SimulatedTransportAddress : public TransportAddress
{
public:
    SimulatedTransportAddress * clone() const;
    int GetAddr() const;
    bool operator==(const SimulatedTransportAddress &other) const;
    inline bool operator!=(const SimulatedTransportAddress &other) const
    {
        return !(*this == other);            
    }
private:
    SimulatedTransportAddress(int addr);
    
    int addr;
    friend class SimulatedTransport;
};

class SimulatedTransport :
    public TransportCommon<SimulatedTransportAddress>
{
    typedef std::function<bool (TransportReceiver*, int,
                                TransportReceiver*, int,
                                Message &, uint64_t &delay)> filter_t;
public:
    SimulatedTransport();
    ~SimulatedTransport();
    void Register(TransportReceiver *receiver,
                  const transport::Configuration &config,
                  int replicaIdx);
    void Run();
    void AddFilter(int id, filter_t filter);
    void RemoveFilter(int id);
    int Timer(uint64_t ms, timer_callback_t cb);
    bool CancelTimer(int id);
    void CancelAllTimers();

protected:
    bool SendMessageInternal(TransportReceiver *src,
                             const SimulatedTransportAddress &dstAddr,
                             const Message &m,
                             bool multicast);
    
    SimulatedTransportAddress
    LookupAddress(const transport::Configuration &cfg, int idx);
    const SimulatedTransportAddress *
    LookupMulticastAddress(const transport::Configuration *cfg);
    
private:
    struct QueuedMessage {
        int dst;
        int src;
        string type;
        string msg;
        inline QueuedMessage(int dst, int src,
                             const string &type, const string &msg) :
            dst(dst), src(src), type(type), msg(msg) { }
    };
    struct PendingTimer {
        uint64_t when;
        int id;
        timer_callback_t cb;
    };

    std::deque<QueuedMessage> queue;
    std::map<int,TransportReceiver *> endpoints;
    int lastAddr;
//    std::map<int,int> replicas;
    std::map<int,int> replicaIdxs;
    std::multimap<int,filter_t> filters;
    std::multimap<uint64_t, PendingTimer> timers;
    int lastTimerId;
    uint64_t vtime;
    bool processTimers;
};

#endif  // _LIB_SIMTRANSPORT_H_
