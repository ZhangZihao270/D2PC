#include "tpccTransaction.h"
#include "util.h"
#include <nlohmann/json.hpp>
#include <iostream>


using namespace std;

using json = nlohmann::json;

// uint64_t Rand(int min, int max)
// {
//     std::random_device rd;
//     std::mt19937 gen(rd());

//     std::uniform_int_distribution<> distrib(min, max);

//     int random_number = distrib(gen);
//     return random_number;
// }

NewOrder::NewOrder(Client *c)
{
    client = c;
}

void NewOrder::GenInputData(uint64_t  ware_id, bool local)
{
    w_id_ = ware_id;
    d_id_ = UtilFunc::Rand(1, g_dist_per_ware);
	c_id_ = UtilFunc::Rand(1, g_cust_per_dist);
    
    ol_cnt_    = UtilFunc::Rand(5, 15);
	o_entry_d_ = 2023;

    // ol_i_ids_        = new ;
    // ol_supply_w_ids_ = new ;
    // ol_quantities_    = ;

    remote_ = 0;

    // is_local_txn_   = local;
    // local_shard_id_ = w_id_ - 1;
    
    // uint64_t remote_w_id = 0;
    // if (g_warehouse_num == 1)
    //     remote_w_id = w_id_;
    // else
    //     remote_w_id = Rand(1, g_warehouse_num);
        // while ((remote_w_id = UtilFunc::URand(1, g_warehouse_num, thread_id)) == w_id_){}


    for (uint32_t i = 0; i < ol_cnt_; i++)
    {
        ol_i_ids_.push_back(UtilFunc::Rand(8191, g_item_num));

        if(local){
            ol_supply_w_ids_.push_back(w_id_);
            remote_ = 0;
        } else {
            uint64_t rand = UtilFunc::Rand(1, g_ware_num);
            ol_supply_w_ids_.push_back(rand);
            remote_ = 1;
        }
        ol_quantities_[i] = UtilFunc::Rand(1, 10);
    }

	// // 同一个订单的不同orderline中不能有重复的item
	// for (uint32_t i = 0; i < ol_cnt_; i ++) 
    // {
    //     bool duplicate = false;
    //     do
    //     {
    //         duplicate = false;

    //         for (uint32_t j = 0; j < i; j++) {
	// 		    if (ol_i_ids_[i] == ol_i_ids_[j]) {
    //                 duplicate = true;
    //                 break;
	// 		    }
	// 	    }

    //         if (duplicate) {
    //             ol_i_ids_[i] = UtilFunc::NURand(8191, 1, g_item_num, thread_id);
    //         }
    //     } while (duplicate);
	// }
}

void NewOrder::GenInputData(bool local)
{
    w_id_ = UtilFunc::Rand(1, g_ware_num);
    Debug("ware num: %lu, generate wid: %lu", g_ware_num, w_id_);
    d_id_ = UtilFunc::Rand(1, g_dist_per_ware);
	c_id_ = UtilFunc::Rand(1, g_cust_per_dist);
    
    ol_cnt_    = UtilFunc::Rand(5, 15);
	o_entry_d_ = 2023;

    remote_ = 0;

    // is_local_txn_   = local;
    // local_shard_id_ = w_id_ - 1;
    
    // uint64_t remote_w_id = 0;
    // if (g_warehouse_num == 1)
    //     remote_w_id = w_id_;
    // else
    //     remote_w_id = Rand(1, g_warehouse_num);

    for (uint32_t i = 0; i < ol_cnt_; i++)
    {
        ol_i_ids_.push_back(UtilFunc::Rand(8191, g_item_num));

        if(local){
            ol_supply_w_ids_.push_back(w_id_);
            remote_ = 0;
        } else {
            uint64_t rand = UtilFunc::Rand(1, g_ware_num);
            ol_supply_w_ids_.push_back(rand);
            remote_ = 1;
        }

        uint64_t quantity = UtilFunc::Rand(1, 10);
        ol_quantities_.push_back(quantity);
    }
    
    //TPCC负载按照warehouse划分shard，当前实现每个warehouse一个shard
    //TODO: 如果实现更复杂的分区方式，则需要提供warehouse和shard的映射方式。
    // if (is_local_txn_ == false)
    //     remote_shard_id_ = remote_w_id - 1;
    

	// 同一个订单的不同orderline中不能有重复的item
	// for (uint32_t i = 0; i < ol_cnt_; i ++) 
    // {
    //     bool duplicate = false;
    //     do
    //     {
    //         duplicate = false;

    //         for (uint32_t j = 0; j < i; j++) {
	// 		    if (ol_i_ids_[i] == ol_i_ids_[j]) {
    //                 duplicate = true;
    //                 break;
	// 		    }
	// 	    }

    //         if (duplicate) {
    //             ol_i_ids_[i] = UtilFunc::NURand(8191, 1, g_item_num, thread_id);
    //         }
    //     } while (duplicate);
	// }
}

int NewOrder::RunTxn(vector<vector<queue<uint64_t>>>* neworders)
{
    double   w_tax;
    uint64_t d_next_o_id;
    double   d_tax;
    double   c_discount;
    std::string  c_last;
    std::string  c_credit;
    uint64_t o_id;
    bool status = true;

    int ret;

    /*=======================================================================+
	EXEC SQL SELECT c_discount, c_last, c_credit, w_tax
		INTO :c_discount, :c_last, :c_credit, :w_tax
		FROM customer, warehouse
		WHERE w_id = :w_id AND c_w_id = w_id AND c_d_id = :d_id AND c_id = :c_id;
	+========================================================================*/

    // printf("1\n");

    /** Warehouse:
     *      w_tax **/
    // Debug("read warehouse");

    json w_key_json;
    json w_tuple_json;

    w_key_json["W_ID"] = w_id_;

    std::string w_value;
    std::string w_key = "W" + w_key_json.dump();

    if ((ret = client->Get(w_key, w_value))) {
        Warning("Aborting due to %s %d", w_key.c_str(), ret);
        status = false;
    }

    // Debug("read ware finish");

    if(status && w_value != ""){
        try {
        w_tuple_json = json::parse(w_value);
        w_tax = w_tuple_json["W_TAX"];
        } catch (json::parse_error& e){
            Debug("parse error W");
            // std::cerr << "JSON parse error: " << e.what() << '\n';
        }
        
        // Debug("tax: %f", w_tax);
    } else {
        Debug("W read null");
    }


    /** Customer: 
     *      c_discount, c_last, c_credit **/
    // printf("2\n");

    json c_key_json;
    json c_tuple_json;

    c_key_json["C_ID"] = c_id_;
    c_key_json["C_D_ID"] = d_id_;
    c_key_json["C_W_ID"] = w_id_;

    std::string c_key = "C" + c_key_json.dump();
    std::string c_value = "";

    if ((ret = client->Get(c_key, c_value))) {
        Warning("Aborting due to %s %d", c_key.c_str(), ret);
        status = false;
    }

    // Debug("read customer finish");
    
    if(status && c_value != ""){
        try{
        c_tuple_json = json::parse(c_value);
        c_discount = c_tuple_json["C_DISCOUNT"];
        c_last = c_tuple_json["C_LAST"];
        c_credit = c_tuple_json["C_CREDIT"];
        } catch (json::parse_error& e){
            Debug("parse error C");
            // std::cerr << "JSON parse error: " << e.what() << '\n';
        }
    } else {
        Debug("C read fail");
    }


	/*==================================================+
	EXEC SQL SELECT d_next_o_id, d_tax
		INTO :d_next_o_id, :d_tax
		FROM district WHERE d_id = :d_id AND d_w_id = :w_id;
	EXEC SQL UPDATE district SET d _next_o_id = :d _next_o_id + 1
		WHERE d _id = :d_id AND d _w _id = :w _id ;
	+===================================================*/

    /** District:
     *      d_tax, d_next_o_id **/
    // printf("3\n");

    json d_key_json;
    json d_tuple_json;

    d_key_json["D_ID"] = d_id_;
    d_key_json["D_W_ID"] = w_id_;

    std::string d_key = "D" + d_key_json.dump();
    std::string d_value = "";

    if ((ret = client->Get(d_key, d_value))) {
        Warning("Aborting due to %s %d", d_key.c_str(), ret);
        status = false;
    }

    // Debug("read district finish");

    if(status && d_value != ""){
        try{
        d_tuple_json = json::parse(d_value);
        d_next_o_id = d_tuple_json["D_NEXT_O_ID"];
        d_tax = d_tuple_json["D_TAX"];

        o_id = d_next_o_id;
        d_next_o_id++;

        d_tuple_json["D_NEXT_O_ID"] = d_next_o_id;
        d_value = d_tuple_json.dump();

        client->Put(d_key, d_value);
        } catch (json::parse_error& e){
            Debug("parse error D");
            // std::cerr << "JSON parse error: " << e.what() << '\n';
        }
        
    } else {
        Debug("D read fail");
    }


	/*========================================================================================+
	EXEC SQL INSERT INTO ORDERS (o_id, o_d_id, o_w_id, o_c_id, o_entry_d, o_ol_cnt, o_all_local)
		VALUES (:o_id, :d_id, :w_id, :c_id, :datetime, :o_ol_cnt, :o_all_local);
	+========================================================================================*/
    json o_key_json;
    json o_tuple_json;

    o_key_json["O_ID"] = o_id;
    o_key_json["O_D_ID"] = d_id_;
    o_key_json["O_W_ID"] = w_id_;

    // o_tuple_json["O_ID"] = o_id;
    // o_tuple_json["O_D_ID"] = d_id_;
    // o_tuple_json["O_W_ID"] = w_id_;
    o_tuple_json["O_C_ID"] = c_id_;
    o_tuple_json["O_ENTRY_D"] = o_entry_d_;
    // o_tuple_json["O_CARRIER_ID"] = d_id_;
    o_tuple_json["O_OL_CNT"] = ol_cnt_;
    o_tuple_json["O_ALL_LOCAL"] = remote_;


    std::string o_key = "O" + o_key_json.dump();
    std::string o_value = o_tuple_json.dump();
    
    client->Put(o_key, o_value);

    // Debug("put order");

    // if(neworders == NULL){
    //     Debug("neworders is null");
    // } else {
    //     Debug("neworders size: %d", neworders->size());
    // }
    // Debug("wid: %lu, did: %lu", w_id_, d_id_);
    // Debug("newordres size before: %d", (*neworders)[w_id_ - 1][d_id_ - 1].size());
    (*neworders)[w_id_-1][d_id_-1].push(o_id);
    // Debug("newordres size after: %d", (*neworders)[w_id_-1][d_id_-1].size());
    
    // Debug("add to neworders");
    /*=======================================================+
    EXEC SQL INSERT INTO NEW_ORDER (no_o_id, no_d_id, no_w_id)
        VALUES (:o_id, :d_id, :w_id);
    +=======================================================*/
    json no_key_json;
    json no_tuple_json;

    no_key_json["NO_O_ID"] = o_id;
    no_key_json["NO_D_ID"] = d_id_;
    no_key_json["NO_W_ID"] = w_id_;

    std::string no_key = "NO" + no_key_json.dump();
    
    client->Put(no_key, "");

    Debug("Add NewOrder: %s", no_key.c_str());

    uint64_t ol_i_id        = 0;
    uint64_t ol_supply_w_id = 0;
    uint64_t ol_quantity    = 0;

    Debug("New order ol number: %d", ol_cnt_);
    for (uint64_t  ol_number = 0; ol_number < ol_cnt_; ol_number++)
    {
        ol_i_id        = ol_i_ids_[ol_number];
        ol_supply_w_id = ol_supply_w_ids_[ol_number];
        ol_quantity    = ol_quantities_[ol_number];

        /*===========================================+
        EXEC SQL SELECT i_price, i_name , i_data
            INTO :i_price, :i_name, :i_data
            FROM item
            WHERE i_id = :ol_i_id;
        +===========================================*/
        // printf("6.1\n");
        json i_key_json;
        json i_tuple_json;

        uint64_t i_price;
        std::string   i_name;
        std::string   i_data;

        i_key_json["I_ID"] = ol_i_id;

        std::string i_key = "I" + i_key_json.dump();
        std::string i_value = "";

        if ((ret = client->Get(i_key, i_value))) {
            // Debug("get fail");
            Warning("Aborting due to %s %d", i_key.c_str(), ret);
            status = false;
        }

        if(status && i_value != ""){
            try{
            i_tuple_json = json::parse(i_value);
            i_price = i_tuple_json["I_PRICE"];
            i_name = i_tuple_json["I_NAME"];
            i_data = i_tuple_json["I_DATA"];
            } catch (json::parse_error& e){
            Debug("parse error I");
            // std::cerr << "JSON parse error: " << e.what() << '\n';
            } 
        }else {
            Debug("I read fail");
        }
            

		/*==============================================================+
		EXEC SQL SELECT s_quantity, s_data,
				s_dist_01, s_dist_02, s_dist_03, s_dist_04, s_dist_05,
				s_dist_06, s_dist_07, s_dist_08, s_dist_09, s_dist_10
			INTO :s_quantity, :s_data,
				:s_dist_01, :s_dist_02, :s_dist_03, :s_dist_04, :s_dist_05,
				:s_dist_06, :s_dist_07, :s_dist_08, :s_dist_09, :s_dist_10
			FROM stock
			WHERE s_i_id = :ol_i_id AND s_w_id = :ol_supply_w_id;
		EXEC SQL UPDATE stock SET s_quantity = :s_quantity
			WHERE s_i_id = :ol_i_id
			AND s_w_id = :ol_supply_w_id;
		+==============================================================*/
        // printf("6.2 thread id: %ld\n", txn_identifier_->thread_id_);
        json s_key_json;
        json s_tuple_json;

        uint64_t s_quantity;
        std::string    s_data;
        std::string    s_dist_info;

        s_key_json["S_I_ID"] = ol_i_id;
        s_key_json["S_W_ID"] = ol_supply_w_id;

        std::string s_key = "S" + s_key_json.dump();
        std::string s_value = "";

        if ((ret = client->Get(s_key, s_value))) {
            Warning("Aborting due to %s %d", s_key.c_str(), ret);
            status = false;
        }

        // Debug("read stock success");

        if(status && s_value != ""){
            try{
            s_tuple_json = json::parse(s_value);
            s_quantity = s_tuple_json["S_QUANTITY"];
            s_data = s_tuple_json["S_DATA"];

            std::string s;
            if(d_id_ < 10){
                s = "S_DIST_0" + std::to_string(d_id_);
            } else {
                s = "S_DIST_10";
            }
            Debug("dist id %s", s.c_str());
            s_dist_info = s_tuple_json[s];

            Debug("quantity: %lu", s_quantity);
            if (s_quantity > ol_quantity + 10)
            {
                s_quantity = s_quantity - ol_quantity;
            } 
            else
            {
                // why add 91 ???
                s_quantity = s_quantity - ol_quantity + 91;
            }

            s_tuple_json["S_QUANTITY"] = s_quantity;
            Debug("quantity: %lu", s_quantity);
            // s_value = s_tuple_json.dump();

            client->Put(s_key, s_value);

            } catch (json::parse_error& e){
            Debug("parse error S");
            // std::cerr << "JSON parse error: " << e.what() << '\n';
        } 
        }else {
            Debug("S read fail");
        }

            // Debug("parse stock success");
            
            
		/*====================================================+
		EXEC SQL INSERT
			INTO order_line(ol_o_id, ol_d_id, ol_w_id, ol_number,
				ol_i_id, ol_supply_w_id,
				ol_quantity, ol_amount, ol_dist_info)
			VALUES(:o_id, :d_id, :w_id, :ol_number,
				:ol_i_id, :ol_supply_w_id,
				:ol_quantity, :ol_amount, :ol_dist_info);
		+====================================================*/
        json ol_key_json;
        json ol_tuple_json;

        ol_key_json["OL_O_ID"] = o_id;
        ol_key_json["OL_D_ID"] = d_id_;
        ol_key_json["OL_W_ID"] = w_id_;
        ol_key_json["OL_NUMBER"] = ol_number;

        // ol_key_json["OL_O_ID"] = o_id;
        // ol_key_json["OL_D_ID"] = d_id_;
        // ol_key_json["OL_W_ID"] = w_id_;
        // ol_key_json["OL_NUMBER"] = ol_number;
        ol_tuple_json["OL_I_ID"] = ol_i_id;
        ol_tuple_json["OL_SUPPLY_W_ID"] = ol_supply_w_id;
        // ol_key_json["OL_DELIVERY_D"] = w_id_;
        ol_tuple_json["OL_QUANTITY"] = ol_quantity;
        ol_tuple_json["OL_DIST_INFO"] = s_dist_info;


        uint64_t ol_amount = 0;
        ol_amount = ol_quantity * i_price * (1 + w_tax + d_tax) * (1 - c_discount);
        ol_tuple_json["OL_AMOUNT"] = ol_amount;


        std::string ol_key = "OL" + ol_key_json.dump();


        std::string ol_value = ol_tuple_json.dump();

        
        client->Put(ol_key, ol_value); 
    }
    return ret;
}



Payment::Payment(Client *c)
{
    client = c;
}


void Payment::GenInputData(uint64_t  ware_id, bool local)
{
    w_id_ = ware_id;
    d_id_ = UtilFunc::Rand(1, g_dist_per_ware);
    
    h_date_   = 2023;
    h_amount_ = UtilFunc::Rand(1, 5000);
    
    by_last_name_ = false;

    // is_local_txn_   = local;
    // local_shard_id_ = w_id_; 

    if(local){
        c_w_id_ = w_id_;
        c_d_id_ = d_id_;
    } else {
        // uint64_t rand = UtilFunc::Rand(1, 100);
        // if (rand > distributed_txn_ratio)
        // {
        //     c_w_id_ = w_id_;
        //     c_d_id_ = d_id_;
        // }
        // else
        // {
            c_w_id_ = UtilFunc::Rand(1, g_ware_num);
            c_d_id_ = UtilFunc::Rand(1, g_dist_per_ware);
        // }
    }

    if (!by_last_name_)
    {
        c_id_ = UtilFunc::Rand(1, g_cust_per_dist);
    }
}

void Payment::GenInputData(bool local)
{
    w_id_ = UtilFunc::Rand(1, g_ware_num);
    Debug("ware num: %lu, generate wid: %lu", g_ware_num, w_id_);
    d_id_ = UtilFunc::Rand(1, g_dist_per_ware);
    
    h_date_   = 2023;
    h_amount_ = UtilFunc::Rand(1, 5000);
    
    by_last_name_ = false;


    // is_local_txn_   = local;
    // local_shard_id_ = w_id_; 

    if(local){
        c_w_id_ = w_id_;
        c_d_id_ = d_id_;
    } else {
        // uint64_t rand = UtilFunc::Rand(1, 100);
        // if (rand > distributed_txn_ratio)
        // {
        //     c_w_id_ = w_id_;
        //     c_d_id_ = d_id_;
        // }
        // else
        // {
            c_w_id_ = UtilFunc::Rand(1, g_ware_num);
            c_d_id_ = UtilFunc::Rand(1, g_dist_per_ware);
        // }
    }

    if (!by_last_name_)
    {
        c_id_ = UtilFunc::Rand(1, g_cust_per_dist);
    }
}

int Payment::RunTxn()
{
    int ret;

    uint64_t w_ytd;
    std::string w_name;


    uint64_t d_ytd;
    std::string d_name;
  

    uint64_t c_balance;
    uint64_t c_ytd_payment;
    uint64_t c_payment_cnt;
    std::string c_credit;

    bool status = true;


    // uint64_t h_id;
    // std::string h_date;



	/*====================================================+
    	EXEC SQL UPDATE warehouse SET w_ytd = w_ytd + :h_amount
		WHERE w_id=:w_id;
	+====================================================*/
	/*======================================================
		EXEC SQL SELECT w_street_1, w_street_2, w_city, w_state, w_zip, w_name
		INTO :w_street_1, :w_street_2, :w_city, :w_state, :w_zip, :w_name
		FROM warehouse
		WHERE w_id=:w_id;
	+===================================================================*/

    Debug("read warehouse");
    json w_key_json;
    json w_tuple_json;

    w_key_json["W_ID"] = w_id_;

    std::string w_key = "W" + w_key_json.dump();
    std::string w_value = "";

    if ((ret = client->Get(w_key, w_value))) {
        Warning("Aborting due to %s %d", w_key.c_str(), ret);
        status = false;
    }

    Debug("read warehouse finished");

    if(status && w_value != ""){
        try{
        w_tuple_json = json::parse(w_value);
        w_ytd = w_tuple_json["W_YTD"];
        w_ytd += h_amount_;
        w_name = w_tuple_json["W_NAME"];

        w_tuple_json["W_YTD"] = w_ytd;
        w_value = w_tuple_json.dump();

        client->Put(w_key, w_value);
        } catch (json::parse_error& e){
            Debug("parse error W");
            // std::cerr << "JSON parse error: " << e.what() << '\n';
        }
        
    } else {
            Debug("W read fail");
        }


	/*=====================================================+
		EXEC SQL UPDATE district SET d_ytd = d_ytd + :h_amount
		WHERE d_w_id=:w_id AND d_id=:d_id;
	+=====================================================*/
	/*====================================================================+
		EXEC SQL SELECT d_street_1, d_street_2, d_city, d_state, d_zip, d_name
		INTO :d_street_1, :d_street_2, :d_city, :d_state, :d_zip, :d_name
		FROM district
		WHERE d_w_id=:w_id AND d_id=:d_id;
	+====================================================================*/
    json d_key_json;
    json d_tuple_json;

    d_key_json["D_ID"] = d_id_;
    d_key_json["D_W_ID"] = w_id_;

    std::string d_key = "D" + d_key_json.dump();
    std::string d_value = "";

    if ((ret = client->Get(d_key, d_value))) {
        Warning("Aborting due to %s %d", d_key.c_str(), ret);
        status = false;
    }

    if(status && d_value != ""){
        try{
        d_tuple_json = json::parse(d_value);
        d_ytd = d_tuple_json["D_YTD"];
        d_ytd += h_amount_;
        d_name = d_tuple_json["D_NAME"];

        d_tuple_json["D_YTD"] = d_ytd;
        d_value = d_tuple_json.dump();

        client->Put(d_key, d_value);
        } catch (json::parse_error& e){
            Debug("parse error D");
            // std::cerr << "JSON parse error: " << e.what() << '\n';
        }
        
    } else {
            Debug("D read fail");
        }


    // if (by_last_name_)
    // {
    //     ;
    // }
    // else
    // {
        /*=====================================================================+
			EXEC SQL SELECT c_first, c_middle, c_last, c_street_1, c_street_2,
			c_city, c_state, c_zip, c_phone, c_credit, c_credit_lim,
			c_discount, c_balance, c_since
			INTO :c_first, :c_middle, :c_last, :c_street_1, :c_street_2,
			:c_city, :c_state, :c_zip, :c_phone, :c_credit, :c_credit_lim,
			:c_discount, :c_balance, :c_since
			FROM customer
			WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_id=:c_id;
		+======================================================================*/

        json c_key_json;
        json c_tuple_json;

        c_key_json["C_ID"] = c_id_;
        c_key_json["C_D_ID"] = c_d_id_;
        c_key_json["C_W_ID"] = c_w_id_;

        std::string c_key = "C" + c_key_json.dump();
        std::string c_value = "";

        if ((ret = client->Get(c_key, c_value))) {
            Warning("Aborting due to %s %d", c_key.c_str(), ret);
            status = false;
        }
        
        if(status && c_value != ""){
            try{
            c_tuple_json = json::parse(c_value);
            c_balance = c_tuple_json["C_BALANCE"];
            c_balance -= h_amount_;

            c_ytd_payment = c_tuple_json["C_YTD_PAYMENT"];
            c_ytd_payment += h_amount_;

            c_payment_cnt = c_tuple_json["C_PAYMENT_CNT"];
            c_payment_cnt++;

            c_credit = c_tuple_json["C_CREDIT"];

            c_tuple_json["C_BALANCE"] = c_balance;
            c_tuple_json["C_YTD_PAYMENT"] = c_ytd_payment;
            c_tuple_json["C_PAYMENT_CNT"] = c_payment_cnt;

            c_value = c_tuple_json.dump();
            client->Put(c_key, c_value);
            } catch (json::parse_error& e){
            Debug("parse error C");
            // std::cerr << "JSON parse error: " << e.what() << '\n';
        } 
        } else {
            Debug("C read fail");
        }
    // }
            

            /*======================================================================+
                EXEC SQL UPDATE customer SET c_balance = :c_balance, c_data = :c_new_data
                WHERE c_w_id = :c_w_id AND c_d_id = :c_d_id AND c_id = :c_id;
            +======================================================================*/

            
    

// 	/*=============================================================================+
// 	  EXEC SQL INSERT INTO
// 	  history (h_c_d_id, h_c_w_id, h_c_id, h_d_id, h_w_id, h_date, h_amount, h_data)
// 	  VALUES (:c_d_id, :c_w_id, :c_id, :d_id, :w_id, :datetime, :h_amount, :h_data);
// 	  +=============================================================================*/
//     json h_key_json;
//     json h_tuple_json;

//     h_key_json["H_ID"] = h_id;
//     h_key_json["H_C_ID"] = d_id_;
//     h_key_json["H_C_D_ID"] = w_id_;
//     h_key_json["H_C_W_ID"] = o_id;
//     h_key_json["H_D_ID"] = d_id_;
//     h_key_json["H_W_ID"] = w_id_;
//     h_key_json["H_DATE"] = o_id;
//     h_key_json["H_AMOUNT"] = d_id_;
//     h_key_json["H_DATA"] = w_id_;


//     std::string o_key = o_key_json.dump();
//     std::string o_value = o_tuple_json.dump();
    
//     client->Put(o_key, o_value);
    
// #if TPCC_INSERT    
//     strncpy(h_data, w_name, 10);
//     strcpy(&h_data[10], "    ");
//     strncpy(&h_data[14], d_name, 10);
//     h_data[24] = '\0';

//     h_id = g_schema->FetchIncID(HISTORY_T);

//     new_tuple = new Tuple(g_schema->GetTupleSize(HISTORY_T));
//     g_schema->SetColumnValue(HISTORY_T, H_ID, new_tuple->tuple_data_, (ColumnData)&h_id);
//     g_schema->SetColumnValue(HISTORY_T, H_W_ID, new_tuple->tuple_data_, (ColumnData)&w_id_);
//     g_schema->SetColumnValue(HISTORY_T, H_D_ID, new_tuple->tuple_data_, (ColumnData)&d_id_);
//     g_schema->SetColumnValue(HISTORY_T, H_C_ID, new_tuple->tuple_data_, (ColumnData)&c_id_);
//     g_schema->SetColumnValue(HISTORY_T, H_C_W_ID, new_tuple->tuple_data_, (ColumnData)&c_w_id_);
//     g_schema->SetColumnValue(HISTORY_T, H_C_D_ID, new_tuple->tuple_data_, (ColumnData)&c_d_id_);
//     g_schema->SetColumnValue(HISTORY_T, H_DATE, new_tuple->tuple_data_, (ColumnData)&h_date_);
//     g_schema->SetColumnValue(HISTORY_T, H_DATA, new_tuple->tuple_data_, (ColumnData)&h_data);
//     g_schema->SetColumnValue(HISTORY_T, H_AMOUNT, new_tuple->tuple_data_, (ColumnData)&h_amount_);

//     rc = txn_context_->AccessTuple(AccessType::INSERT_AT, HISTORY_T, w_id_-1, new_tuple, operate_tuple);
    
//     if (rc == RC_ABORT)
//         return rc;

//     index = g_schema->GetIndex(H_PK_INDEX, w_id_-1);
//     index_attr[0] = &h_id;
//     index_key = g_schema->GetIndexKey(H_PK_INDEX, index_attr);

//     index->IndexInsert(index_key, new_tuple);

// #endif


    return ret;    

}



Delivery::Delivery(Client *c)
{
    client = c;
}

void Delivery::GenInputData(bool local)
{
    w_id_ = UtilFunc::Rand(1, g_ware_num);
    Debug("ware num: %lu, generate wid: %lu", g_ware_num, w_id_);
    o_carrier_id_ = UtilFunc::Rand(1, g_dist_per_ware);
    ol_delivery_d_ = 2024;

    //Delivery事务一定是本地事务
    // is_local_txn_ = true;
    // local_shard_id_ = w_id_ - 1;
}

void Delivery::GenInputData(uint64_t  ware_id, bool local)
{
    w_id_ = ware_id;
    o_carrier_id_ = UtilFunc::Rand(1, g_dist_per_ware);
    ol_delivery_d_ = 2024;

    //Delivery事务一定是本地事务
    // is_local_txn_ = true;
    // local_shard_id_ = w_id_ - 1;
}

int Delivery::RunTxn(vector<vector<queue<uint64_t>>>* neworders)
{
    // //保证同一个warehouse同一时刻只能运行一个delivery事务
    // while (!ATOM_CAS(((TPCCWorkload*)g_workload)->deliverying[w_id_], false, true))
    // {
    //     ;
    // }
    
    int ret = 0;
    bool status = true;
    int ol_cnt_ = 0;

    bool     find[g_dist_per_ware + 1];
    uint64_t d_id;


    for (d_id = 1; d_id <= g_dist_per_ware; d_id++)
    {
        find[d_id] = false;

        /********************************************************
         * 找到该district下，最老还未delivery的order id。
         * 将对应的new_order元组删除
         *******************************************************/
        // uint64_t o_id = ((TPCCWorkload*)g_workload)->undelivered_order[w_id_][d_id];

        uint64_t o_id = 0;
        if(!(*neworders)[w_id_-1][d_id-1].empty()){
            o_id = (*neworders)[w_id_-1][d_id-1].front();
            (*neworders)[w_id_-1][d_id-1].pop();
        
        Debug("dist id: %lu", d_id);

        json no_key_json;

        no_key_json["NO_O_ID"] = o_id;
        no_key_json["NO_D_ID"] = d_id;
        no_key_json["NO_W_ID"] = w_id_;

        std::string no_key = "NO" + no_key_json.dump();
        std::string no_value = "";

        if ((ret = client->Get(no_key, no_value))) {
            Warning("Aborting due to %s %d", no_key.c_str(), ret);
            status = false;
            break;
        }

        if(no_value == ""){
            find[d_id] = true;
            client->Put(no_key, "deleted");
        }

        /*************************************************
         * 根据w_id d_id o_id查找对应的order
         * 获取o_c_id，并更新o_carrier_id_
         *************************************************/
        
        json o_key_json;
        json o_tuple_json;

        o_key_json["O_ID"] = o_id;
        o_key_json["O_D_ID"] = d_id;
        o_key_json["O_W_ID"] = w_id_;

        std::string o_key = "O" + o_key_json.dump();
        std::string o_value = "";
        uint64_t o_c_id = 0;

        if ((ret = client->Get(o_key, o_value))) {
            Warning("Aborting due to %s %d", o_key.c_str(), ret);
            status = false;
        }

        if(status && o_value != ""){
            try{
            o_tuple_json = json::parse(o_value);
            o_c_id = o_tuple_json["O_C_ID"];
            o_tuple_json["O_CARRIER_ID"] = o_carrier_id_;
            ol_cnt_ = o_tuple_json["O_OL_CNT"];
            Debug("new order ol number: %d", ol_cnt_);
            o_value = o_tuple_json.dump();

            client->Put(o_key, o_value);
            } catch (json::parse_error& e){
            Debug("parse error O");
            // std::cerr << "JSON parse error: " << e.what() << '\n';
        } 
        }else {
            Debug("O read fail");
        }
            

        /*************************************************************
         * 查找Order对应的Order-line，
         * 对每个Order-line 更新 ol_delivery_d 属性，计算ol_amount总和
         ************************************************************/
        uint64_t ol_total = 0;

        for (uint64_t  ol_number = 0; ol_number < ol_cnt_; ol_number++)
        {
            json ol_key_json;
            json ol_tuple_json;

            ol_key_json["OL_O_ID"] = o_id;
            ol_key_json["OL_D_ID"] = d_id;
            ol_key_json["OL_W_ID"] = w_id_;
            ol_key_json["OL_NUMBER"] = ol_number;

            std::string ol_key = "OL" + ol_key_json.dump();
            std::string ol_value = "";

            if ((ret = client->Get(ol_key, ol_value))) {
                Warning("Aborting due to %s %d", ol_key.c_str(), ret);
                status = false;
            }

            if(status && ol_value != ""){
                try{
                ol_tuple_json = json::parse(ol_value);
                
                uint64_t ol_amount = ol_tuple_json["OL_AMOUNT"];
                ol_total += ol_amount;

                ol_tuple_json["OL_DELIVERY_D"] = ol_delivery_d_;
                ol_value = ol_tuple_json.dump();
                client->Put(ol_key, ol_value);
                } catch (json::parse_error& e){
            Debug("parse error OL");
            // std::cerr << "JSON parse error: " << e.what() << '\n';
        }
            } else {
            Debug("OL read fail");
        }
        }

        /**********************************
         * 根据order的O_C_ID属性，确定订单所属customer。
         * 更新c_balance属性，+ ol_total
         **********************************/
        
        json c_key_json;
        json c_tuple_json;

        c_key_json["C_ID"] = o_c_id;
        c_key_json["C_D_ID"] = d_id;
        c_key_json["C_W_ID"] = w_id_;

        std::string c_key = "C" + c_key_json.dump();
        std::string c_value = "";

        if ((ret = client->Get(c_key, c_value))) {
            Warning("Aborting due to %s %d", c_key.c_str(), ret);
            status = false;
        }

        if(status && c_value != ""){
            try{
            c_tuple_json = json::parse(c_value);
            // c_discount = c_tuple_json["C_DISCOUNT"];
            // c_last = c_tuple_json["C_LAST"];
            // c_credit = c_tuple_json["C_CREDIT"];
            
            uint64_t c_balance = 0;
            c_balance = c_tuple_json["C_BALANCE"];
            c_balance += ol_total;

            c_tuple_json["C_BALANCE"] = c_balance;
            c_value = c_tuple_json.dump();
            client->Put(c_key, c_value);
            } catch (json::parse_error& e){
            Debug("parse error C");
            // std::cerr << "JSON parse error: " << e.what() << '\n';
        }
        } else {
            Debug("C read fail");
        }
        break;
        }
    }
    

//     //对于处理了new_order的district，将其undelivered的order id + 1
//     for (uint64_t  i = 1; i <= g_dist_per_ware; i++)
//     {
//         if (find[i])
//             ((TPCCWorkload*)g_workload)->undelivered_order[w_id_][i]++;
//     }


// commit_txn:

//     ATOM_CAS(((TPCCWorkload*)g_workload)->deliverying[w_id_], true, false);
//     return RC_COMMIT;

// abort_txn:

//     ATOM_CAS(((TPCCWorkload*)g_workload)->deliverying[w_id_], true, false);
//     return RC_ABORT;
    return ret;

}