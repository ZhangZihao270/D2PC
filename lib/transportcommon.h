

#ifndef _LIB_TRANSPORTCOMMON_H_
#define _LIB_TRANSPORTCOMMON_H_

#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/transport.h"

#include <map>
#include <unordered_map>

template <typename ADDR>
class TransportCommon : public Transport
{
    
public:
    TransportCommon() 
    {
        replicaAddressesInitialized = false;
    }

    virtual
    ~TransportCommon()
    {
        for (auto &kv : canonicalConfigs) {
            delete kv.second;
        }
    }
    
    virtual bool
    SendMessage(TransportReceiver *src, const TransportAddress &dst,
                const Message &m)
    {
        const ADDR &dstAddr = dynamic_cast<const ADDR &>(dst);
        return SendMessageInternal(src, dstAddr, m, false);
    }

    virtual bool
    SendMessageToReplica(TransportReceiver *src, int replicaIdx,
                         const Message &m)
    {
        const transport::Configuration *cfg = configurations[src];
        ASSERT(cfg != NULL);

        if (!replicaAddressesInitialized) {
            LookupAddresses();
        }
        
        auto kv = replicaAddresses[cfg].find(replicaIdx);
        ASSERT(kv != replicaAddresses[cfg].end());
        
        return SendMessageInternal(src, kv->second, m, false);
    }

    virtual bool
    SendMessageToAll(TransportReceiver *src, const Message &m)
    {
        const transport::Configuration *cfg = configurations[src];
        ASSERT(cfg != NULL);

        if (!replicaAddressesInitialized) {
            LookupAddresses();
        }

        auto kv = multicastAddresses.find(cfg);
        if (kv != multicastAddresses.end()) {
            // Send by multicast if we can
            return SendMessageInternal(src, kv->second, m, true);
        } else {
            // ...or by individual messages to every replica if not
            const ADDR &srcAddr = dynamic_cast<const ADDR &>(src->GetAddress());
            for (auto & kv2 : replicaAddresses[cfg]) {
                if (srcAddr == kv2.second) {
                    continue;
                }
                if (!SendMessageInternal(src, kv2.second, m, false)) {
                    return false;
                }
            }
            return true;
        }
    }
    
protected:
    virtual bool SendMessageInternal(TransportReceiver *src,
                                     const ADDR &dst,
                                     const Message &m,
                                     bool multicast = false) = 0;
    virtual ADDR LookupAddress(const transport::Configuration &cfg,
                               int replicaIdx) = 0;
    virtual const ADDR *
    LookupMulticastAddress(const transport::Configuration *cfg) = 0;

    std::unordered_map<transport::Configuration,
                       transport::Configuration *> canonicalConfigs;
    std::map<TransportReceiver *,
             transport::Configuration *> configurations;
    std::map<const transport::Configuration *,
             std::map<int, ADDR> > replicaAddresses;
    std::map<const transport::Configuration *,
             std::map<int, TransportReceiver *> > replicaReceivers;
    std::map<const transport::Configuration *, ADDR> multicastAddresses;
    bool replicaAddressesInitialized;

    virtual transport::Configuration *
    RegisterConfiguration(TransportReceiver *receiver,
                          const transport::Configuration &config,
                          int replicaIdx)
    {
        ASSERT(receiver != NULL);

        // Have we seen this configuration before? If so, get a
        // pointer to the canonical copy; if not, create one. This
        // allows us to use that pointer as a key in various
        // structures. 
        transport::Configuration *canonical
            = canonicalConfigs[config];
        if (canonical == NULL) {
            canonical = new transport::Configuration(config);
            canonicalConfigs[config] = canonical;
        }

        // Record configuration
        configurations.insert(std::make_pair(receiver, canonical));

        // If this is a replica, record the receiver
        if (replicaIdx != -1) {
            replicaReceivers[canonical].insert(std::make_pair(replicaIdx,
                                                              receiver));
        }

        // Mark replicaAddreses as uninitalized so we'll look up
        // replica addresses again the next time we send a message.
        replicaAddressesInitialized = false;

        return canonical;
    }

    virtual void
    LookupAddresses()
    {
        // Clear any existing list of addresses
        replicaAddresses.clear();
        multicastAddresses.clear();

        // For every configuration, look up all addresses and cache
        // them.
        for (auto &kv : canonicalConfigs) {
            transport::Configuration *cfg = kv.second;

            for (int i = 0; i < cfg->n; i++) {
                const ADDR addr = LookupAddress(*cfg, i);
                replicaAddresses[cfg].insert(std::make_pair(i, addr));
            }

            // And check if there's a multicast address
            if (cfg->multicast()) {
                const ADDR *addr = LookupMulticastAddress(cfg);
                if (addr) {
                    multicastAddresses.insert(std::make_pair(cfg, *addr));
                    delete addr;
                }
            }
        }
        
        replicaAddressesInitialized = true;
    }
};

#endif // _LIB_TRANSPORTCOMMON_H_