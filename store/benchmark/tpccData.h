#ifndef EXECUTOR_LOAD_THREAD_H_
#define EXECUTOR_LOAD_THREAD_H_

// #include "config.h"
#include <cstdint>
#include <string>

using namespace std;

class TPCCData
{
private:
    
    std::string GenerateItem();
    std::string GenerateWarehouse();
    std::string GenerateDistrict();
    std::string GenerateStock();
    std::string GenerateCustomer(uint64_t c_id);
    // void GenerateOrder(uint64_t  w_id, uint64_t d_id);
    // void GenerateHistory(uint64_t  w_id, uint64_t d_id, uint64_t c_id);
    

    uint64_t min_w_id_;
    uint64_t max_w_id_;


public:
    TPCCData(uint64_t min_w_id, uint64_t max_w_id);
    ~TPCCData();
    
    void Generate();
};



#endif