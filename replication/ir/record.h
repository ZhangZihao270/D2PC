

#ifndef _IR_RECORD_H_
#define _IR_RECORD_H_

#include <map>
#include <string>
#include <utility>

#include "lib/assert.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "replication/common/request.pb.h"
#include "replication/common/viewstamp.h"
#include "replication/ir/ir-proto.pb.h"

namespace replication {
namespace ir {

typedef std::pair<uint64_t, uint64_t> opid_t;

struct RecordEntry
{
    view_t view;
    opid_t opid;
    proto::RecordEntryState state;
    proto::RecordEntryType type;
    Request request;
    std::string result;

    RecordEntry() { result = ""; }
    RecordEntry(const RecordEntry &x)
        : view(x.view),
          opid(x.opid),
          state(x.state),
          type(x.type),
          request(x.request),
          result(x.result) {}
    RecordEntry(view_t view, opid_t opid, proto::RecordEntryState state,
                proto::RecordEntryType type, const Request &request,
                const std::string &result)
        : view(view),
          opid(opid),
          state(state),
          type(type),
          request(request),
          result(result) {}
    virtual ~RecordEntry() {}
};

class Record
{
public:
    // Use the copy-and-swap idiom to make Record moveable but not copyable
    // [1]. We make it non-copyable to avoid unnecessary copies.
    //
    // [1]: https://stackoverflow.com/a/3279550/3187068
    Record(){};
    Record(const proto::RecordProto &record_proto);
    Record(Record &&other) : Record() { swap(*this, other); }
    Record(const Record &) = delete;
    Record &operator=(const Record &) = delete;
    Record &operator=(Record &&other) {
        swap(*this, other);
        return *this;
    }
    friend void swap(Record &x, Record &y) {
        std::swap(x.entries, y.entries);
    }

    RecordEntry &Add(const RecordEntry& entry);
    RecordEntry &Add(view_t view, opid_t opid, const Request &request,
                     proto::RecordEntryState state,
                     proto::RecordEntryType type);
    RecordEntry &Add(view_t view, opid_t opid, const Request &request,
                     proto::RecordEntryState state, proto::RecordEntryType type,
                     const std::string &result);
    RecordEntry *Find(opid_t opid);
    bool SetStatus(opid_t opid, proto::RecordEntryState state);
    bool SetResult(opid_t opid, const std::string &result);
    bool SetRequest(opid_t opid, const Request &req);
    void Remove(opid_t opid);
    bool Empty() const;
    void ToProto(proto::RecordProto *proto) const;
    const std::map<opid_t, RecordEntry> &Entries() const;

private:
    std::map<opid_t, RecordEntry> entries;
};

}      // namespace ir
}      // namespace replication
#endif  /* _IR_RECORD_H_ */
