#include <vector>
#include <queue>
#include "store/strongstore/client.h"


using namespace std;

// class TpccTransaction
// {
// // protected:
// //     //transaction identification
// //     //thread_id + local_txn_id
// //     TxnIdentifier* txn_identifier_;
    
// //     /*** 事务执行的上下文 ***/
// //     TxnContext*    txn_context_;

// public:
//     TpccTransaction(){}
//     // TpccTransaction(uint64_t local_txn_id);
//     ~TpccTransaction();

//     /* 
//      * 生成事务输入参数
//      * 第一个指定了shard_id，表示事务输入要在该shard_id中
//      * 第二个未指定shard_id，表示随机生成所属的shard
//      */
//     virtual void GenInputData(uint64_t  ware_id, bool local) = 0;
//     virtual void GenInputData(bool local) = 0;

//     //the logic of transaction
//     virtual int RunTxn() = 0;


//     // void InitTxnIdentifier(ThreadID thread_id, uint64_t local_txn_id);
//     // TxnIdentifier* GetTxnIdentifier();

//     // void SetTxnContext(TxnContext* txn_context);

//     //事务类型
//     uint64_t       txn_type_;

//     /* 
//      * 判断事务是否为分布式事务
//      * local_shard_id_表示事务所在shard
//      * remote_shard_id_表示在分布式事务中，访问的另一个shard
//      * 
//      * 注意，目前事务最多访问两个不同的shard，不支持事务访问三个及以上shard
//      */
//     bool     is_local_txn_;
//     uint64_t  local_shard_id_;
//     uint64_t  remote_shard_id_;

    

// //     /* 
// //      * 记录事务日志最末尾所在的LSN
// //      * 目前用于事务预提交策略，避免事务并发控制临界区包含日志持久化过程
// //      */
// // #if  PRE_COMMIT_TXN == true
// //     LogBufID log_buffer_id_;
// //     LogLSN   last_log_lsn_;

// //  #if  DISTRIBUTED_LOG
// //     LogBufID remote_log_buffer_id_;
// //     LogLSN   remote_last_log_lsn_;
// //  #endif
// // #endif
// };

class NewOrder
{
private:

    //input for neworder
    uint64_t  w_id_;
    uint64_t  d_id_;
    uint64_t  c_id_;
    uint64_t  o_entry_d_; // the data of order
    uint64_t  ol_cnt_; // item number
    std::vector<uint64_t> ol_i_ids_; // item id
    std::vector<uint64_t> ol_supply_w_ids_; // the warehouse of item
    std::vector<uint64_t> ol_quantities_; // item quantities

    /*
     * 事务是否访问了其他warehouse的数据，或者说事务是否为分布式事务
     * 0 表示本地事务, 1 表示分布式事务
     */
    uint64_t remote_;    

    Client *client;     
    
public:
    NewOrder(Client *c);
    // NewOrder(TPCCTxnType txn_type);
    // NewOrder(ThreadID thread_id, uint64_t local_txn_id);
    // ~NewOrder();

    int RunTxn(vector<vector<queue<uint64_t>>>* neworders);

    //generate input data for neworder
    void GenInputData(uint64_t  ware_id, bool local);
    void GenInputData(bool local);
};


class Payment
{
private:
    
    uint64_t  w_id_;
    uint64_t  d_id_;
    uint64_t  c_id_;
    uint64_t  c_w_id_; 
    uint64_t  c_d_id_;
	std::string c_last_;
    uint64_t  h_date_;
	uint64_t  h_amount_;

	bool     by_last_name_;

    Client *client;


public:
    Payment(Client *c);
    // Payment(TPCCTxnType txn_type);
    // ~Payment();

    int RunTxn();

    //generate input data for neworder
    void GenInputData(uint64_t  ware_id, bool local);
    void GenInputData(bool local);
};



class Delivery
{
private:
    
    uint64_t w_id_;
    uint64_t o_carrier_id_;
    uint64_t ol_delivery_d_; // delivery date

    Client *client;

public:
    Delivery(Client *c);
    // Delivery(TPCCTxnType txn_type);
    // ~Delivery();

    //generate input data for neworder
    void GenInputData(uint64_t  ware_id, bool local);
    void GenInputData(bool local);

    int RunTxn(vector<vector<queue<uint64_t>>>* neworders);

};