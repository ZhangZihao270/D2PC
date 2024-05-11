// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * common/client.h:
 *   Interface for a multiple shard transactional client.
 *
 **********************************************************************/

#ifndef _CLIENT_API_H_
#define _CLIENT_API_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "store/benchmark/config.h"

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using namespace std;

using json = nlohmann::json;

class Client
{
public:
    Client() {};
    virtual ~Client() {};

    // Begin a transaction.
    virtual void Begin() = 0;

    // Get the value corresponding to key.
    virtual int Get(const std::string &key, std::string &value) = 0;

    virtual int MultiGet(const std::vector<std::string> &keys, std::string &value) = 0;

    // Set the value for the given key.
    virtual int Put(const std::string &key, const std::string &value) = 0;

    // Commit all Get(s) and Put(s) since Begin().
    virtual bool Commit() = 0;
    
    // Abort all Get(s) and Put(s) since Begin().
    virtual void Abort() = 0;

    // Returns statistics (vector of integers) about most recent transaction.
    virtual std::vector<int> Stats() = 0;

    // Sharding logic: Given key, generates a number b/w 0 to nshards-1
    uint64_t key_to_shard(const std::string &key, uint64_t nshards) {
        Debug("key %s", key.c_str());
        uint64_t i;
        if(nshards == 0){
            // int i;
            std::string table;
            std::string key_;
            size_t pos = key.find('{');

            if (pos != std::string::npos) { // 如果找到了'{'字符
                table = key.substr(0, pos); // 截取从开始到'{'之前的子字符串
                key_ = key.substr(pos);
                Debug("table %s", table.c_str());
                Debug("key %s", key_.c_str());
            } else {
                Warning("Wrong format for key %s", key.c_str());
                return 0;
            }

            json key_json; // 假定key_已经被定义为一个json字符串

            if (table == "W") {
                // Debug("Parse warehouse key");
                key_json = json::parse(key_);
                // Debug("Parsed");
                i = key_json["W_ID"];
                // Debug("shard: %lu", i);
            } else if (table == "C") {
                key_json = json::parse(key_);
                i = key_json["C_W_ID"];
            } else if (table == "D") {
                key_json = json::parse(key_);
                i = key_json["D_W_ID"];
            } else if (table == "O") {
                key_json = json::parse(key_);
                i = key_json["O_W_ID"];
            } else if (table == "NO") {
                key_json = json::parse(key_);
                i = key_json["NO_W_ID"];
            } else if (table == "I") {
                key_json = json::parse(key_);
                i = 1; // 注意，在这个case里，我们假设i被设置为0，但没有使用key_json
            } else if (table == "S") {
                key_json = json::parse(key_);
                i = key_json["S_W_ID"];
            } else if (table == "OL") {
                key_json = json::parse(key_);
                i = key_json["OL_W_ID"];
            } else {
                Warning("Unexpected table");
            }

            i = (i - 1) / ware_per_shard;

            // i--;


            // switch(table) {
            //     case "W":
            //         json w_key_json = json::parse(key_);
            //         i = w_key_json["W_ID"];
            //         break;
            //     case "C":
            //         json c_key_json = json::parse(key_);
            //         i = c_key_json["C_W_ID"];
            //         break;
            //     case "D":
            //         json d_key_json = json::parse(key_);
            //         i = d_key_json["D_W_ID"];
            //         break;
            //     case "O":
            //         json o_key_json = json::parse(key_);
            //         i = o_key_json["O_W_ID"];
            //         break;
            //     case "NO":
            //         json no_key_json = json::parse(key_);
            //         i = no_key_json["NO_W_ID"];
            //         break;
            //     case "I":
            //         json i_key_json = json::parse(key_);
            //         i = 0;
            //         break;
            //     case "S":
            //         json s_key_json = json::parse(key_);
            //         i = s_key_json["S_W_ID"];
            //         break;
            //     case "OL":
            //         json ol_key_json = json::parse(key_);
            //         i = ol_key_json["OL_W_ID"];
            //         break;
            //     default:
            //         Warning("Unexpected table");
            // }
        } else {
            uint64_t hash = 5381;
            const char* str = key.c_str();
            for (unsigned int i = 0; i < key.length(); i++) {
                hash = ((hash << 5) + hash) + (uint64_t)str[i];
            }
            i = hash % nshards;
        }

        return i;
    };
};

#endif /* _CLIENT_API_H_ */
