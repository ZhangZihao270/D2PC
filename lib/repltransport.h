

#ifndef _LIB_REPLTRANSPORT_H_
#define _LIB_REPLTRANSPORT_H_

#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>

#include "lib/configuration.h"
#include "lib/transport.h"
#include "lib/transportcommon.h"

class ReplTransportAddress : public TransportAddress {
public:
    // Constructors.
    ReplTransportAddress() {}

    ReplTransportAddress(std::string host, std::string port)
        : host_(std::move(host)), port_(std::move(port)) {}

    ReplTransportAddress(const ReplTransportAddress &other)
        : ReplTransportAddress(other.host_, other.port_) {}

    ReplTransportAddress(ReplTransportAddress &&other)
        : ReplTransportAddress() {
        swap(*this, other);
    }

    ReplTransportAddress &operator=(ReplTransportAddress other) {
        swap(*this, other);
        return *this;
    }

    friend void swap(ReplTransportAddress &x, ReplTransportAddress &y) {
        std::swap(x.host_, y.host_);
        std::swap(x.port_, y.port_);
    }

    // Comparators.
    bool operator==(const ReplTransportAddress &other) const {
        return Key() == other.Key();
    }
    bool operator!=(const ReplTransportAddress &other) const {
        return Key() != other.Key();
    }
    bool operator<(const ReplTransportAddress &other) const {
        return Key() < other.Key();
    }
    bool operator<=(const ReplTransportAddress &other) const {
        return Key() <= other.Key();
    }
    bool operator>(const ReplTransportAddress &other) const {
        return Key() > other.Key();
    }
    bool operator>=(const ReplTransportAddress &other) const {
        return Key() >= other.Key();
    }

    // Getters.
    const std::string& Host() const {
        return host_;
    }

    const std::string& Port() const {
        return port_;
    }

    ReplTransportAddress *clone() const override {
        return new ReplTransportAddress(host_, port_);
    }

    friend std::ostream &operator<<(std::ostream &out,
                                    const ReplTransportAddress &addr) {
        out << addr.host_ << ":" << addr.port_;
        return out;
    }

private:
    std::tuple<const std::string&, const std::string&> Key() const {
        return std::forward_as_tuple(host_, port_);
    }

    std::string host_;
    std::string port_;
};

class ReplTransport : public TransportCommon<ReplTransportAddress> {
public:
    void Register(TransportReceiver *receiver,
                  const transport::Configuration &config,
                  int replicaIdx) override;
    int Timer(uint64_t ms, timer_callback_t cb) override;
    bool CancelTimer(int id) override;
    void CancelAllTimers() override;

    // DeliverMessage(addr, i) delivers the ith queued inbound message to the
    // receiver with address addr. It's possible to send a message to the
    // address of a receiver that hasn't yet registered. In this case,
    // DeliverMessage returns false. Otherwise, it returns true.
    bool DeliverMessage(const ReplTransportAddress& addr, int index);

    // Run timer with id timer_id.
    void TriggerTimer(int timer_id);

    // Launch the REPL.
    void Run();

protected:
    bool SendMessageInternal(TransportReceiver *src,
                             const ReplTransportAddress &dst, const Message &m,
                             bool multicast = false) override;
    ReplTransportAddress LookupAddress(const transport::Configuration &cfg,
                                       int replicaIdx) override;
    const ReplTransportAddress *LookupMulticastAddress(
        const transport::Configuration *cfg) override;

private:
    // Prompt the user for input and either (1) trigger a timer, (2) deliver a
    // message, or (3) quit. RunOne returns true if the user decides to quit.
    bool RunOne();

    // Pretty print the current state of the system. For example, PrintState
    // prints the queued messages for every node in the system.
    void PrintState() const;

    struct QueuedMessage {
        ReplTransportAddress src;
        std::unique_ptr<Message> msg;

        QueuedMessage(ReplTransportAddress src, std::unique_ptr<Message> msg)
            : src(std::move(src)), msg(std::move(msg)) {}
    };

    struct TransportReceiverState {
        // receiver can be null if it has queued messages but hasn't yet been
        // registered with a ReplTransport.
        TransportReceiver *receiver;

        // Queued inbound messages.
        std::vector<QueuedMessage> msgs;
    };

    // receivers_ maps a receiver r's address to r and r's queued messages.
    std::map<ReplTransportAddress, TransportReceiverState> receivers_;

    // timer_id_ is an incrementing counter used to assign timer ids.
    int timer_id_ = 0;

    // timers_ maps timer ids to timers.
    std::map<int, timer_callback_t> timers_;

    // client_id_ is an incrementing counter used to assign addresses to
    // clients. The first client gets address client:0, the next client gets
    // address client:1, etc.
    int client_id_ = 0;

    // A history of all the command issued to this ReplTransport.
    std::vector<std::string> history_;
};

#endif // _LIB_REPLTRANSPORT_H_
