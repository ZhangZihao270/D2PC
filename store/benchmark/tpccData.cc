#include "tpccData.h"
#include "util.h"
#include <stdio.h>
// #include <unistd.h>
#include <string>
#include "store/common/frontend/client.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>

using namespace std;

using json = nlohmann::json;

TPCCData::TPCCData(uint64_t min_w_id, uint64_t max_w_id)
{
    min_w_id_  = min_w_id;
    max_w_id_  = max_w_id;
}

TPCCData::~TPCCData(){}

void TPCCData::Generate(){
    // for each warehourse, there will be a individual data file
    for (uint64_t w_id = min_w_id_; w_id <= max_w_id_; w_id++)
    {
        Debug("wid:%lu", w_id);
        
        std::string kpath = key_path + std::to_string(w_id);
        std::ofstream outFile(kpath);

        Debug("key path:%s", kpath.c_str());

        if (outFile.is_open()) {
            for (uint64_t item_id = 1; item_id <= g_item_num; item_id++)
            {
                //generate item
                json i_key_json;
                i_key_json["I_ID"] = item_id;
                std::string i_key = "I" + i_key_json.dump();
                std::string i_tuple = GenerateItem();

                outFile << i_key << '\n';
                outFile << i_tuple << '\n';
            }

                //generate warehouse
                json w_key_json;
                w_key_json["W_ID"] = w_id;
                std::string w_key = "W" + w_key_json.dump();
                std::string w_tuple = GenerateWarehouse();

                outFile << w_key << '\n';
                outFile << w_tuple << '\n';

                
                for (uint64_t  d_id = 1; d_id <= g_dist_per_ware; d_id++)
                {
                    // generate district for each warehouse
                    json d_key_json;
                    d_key_json["D_ID"] = d_id;
                    d_key_json["D_W_ID"] = w_id;
                    std::string d_key = "D" + d_key_json.dump();
                    std::string d_tuple = GenerateDistrict();

                    outFile << d_key << '\n';
                    outFile << d_tuple << '\n';

                    for (uint64_t  c_id = 1; c_id <= g_cust_per_dist; c_id++)
                    {
                        //generate customer
                        json c_key_json;
                        c_key_json["C_ID"] = c_id;
                        c_key_json["C_D_ID"] = d_id;
                        c_key_json["C_W_ID"] = w_id;
                        std::string c_key = "C" + c_key_json.dump();
                        std::string c_tuple = GenerateCustomer(c_id);

                        outFile << c_key << '\n';
                        outFile << c_tuple << '\n';
                    }
                }

                for (uint64_t  s_i_id = 1; s_i_id <= g_item_num; s_i_id++)
                {
                    //generate stock
                    json s_key_json;
                    s_key_json["S_I_ID"] = s_i_id;
                    s_key_json["S_W_ID"] = w_id;
                    std::string s_key = "S" + s_key_json.dump();
                    std::string s_tuple = GenerateStock();

                    outFile << s_key << '\n';
                    outFile << s_tuple << '\n';
                }
                outFile.close();  // 完成后关闭文件
            }   else {
                std::cerr << "Cannot open file" << std::endl;
            }
            
    } 
}

std::string TPCCData::GenerateItem()
{
        uint64_t i_im_id;
        std::string i_name;
        uint64_t i_price;
        std::string i_data;

        i_im_id = UtilFunc::Rand(1L, 10000L);
        i_name = UtilFunc::MakeAlphaString(14, 24);
        i_price = UtilFunc::Rand(1, 100);
        i_data = UtilFunc::MakeAlphaString(26, 50);
        
        // json i_key_json;
        json i_tuple_json;

        // i_key_json["I_ID"] = item_id;

        i_tuple_json["I_IM_ID"] = i_im_id;
        i_tuple_json["I_NAME"] = i_name;
        i_tuple_json["I_PRICE"] = i_price;
        i_tuple_json["I_DATA"] = i_data;

        // std::string i_key = "ITEM" + i_key_json.dump();
        std::string i_tuple = i_tuple_json.dump();

    return i_tuple;
}

std::string TPCCData::GenerateWarehouse()
{
    std::string w_name = UtilFunc::MakeAlphaString(6, 10);
    std::string w_street = UtilFunc::MakeAlphaString(10, 20);
    std::string w_state = UtilFunc::MakeAlphaString(2, 2);
    std::string w_zip = UtilFunc::MakeAlphaString(9, 9);
    double w_tax =  (double)UtilFunc::Rand(0L, 200L)/1000.0;
    double w_ytd = 300000.00;

    json w_tuple_json;

    w_tuple_json["W_NAME"] = w_name;
    w_tuple_json["W_STREET_1"] = w_street;
    w_tuple_json["W_STREET_2"] = w_street;
    w_tuple_json["W_CITY"] = w_street;
    w_tuple_json["W_STATE"] = w_state;
    w_tuple_json["W_ZIP"] = w_zip;
    w_tuple_json["W_TAX"] = w_tax;
    w_tuple_json["W_YTD"] = w_ytd;

    std::string w_tuple = w_tuple_json.dump();

    return w_tuple;
}

std::string TPCCData::GenerateDistrict()
{
    std::string d_name = UtilFunc::MakeAlphaString(6, 10);
    std::string d_street = UtilFunc::MakeAlphaString(10, 20);
    std::string d_state = UtilFunc::MakeAlphaString(2, 2);
    std::string d_zip =  UtilFunc::MakeNumberString(9, 9);
    double d_tax = (double)UtilFunc::Rand(0L, 200L)/1000.0;
    double d_ytd = 30000.00;
    uint64_t d_next_o_id = 3001;

    json d_tuple_json;

    d_tuple_json["D_NAME"] = d_name;
    d_tuple_json["D_STREET_1"] = d_street;
    d_tuple_json["D_STREET_2"] = d_street;
    d_tuple_json["D_CITY"] = d_street;
    d_tuple_json["D_STATE"] = d_state;
    d_tuple_json["D_ZIP"] = d_zip;
    d_tuple_json["D_TAX"] = d_tax;
    d_tuple_json["D_YTD"] = d_ytd;
    d_tuple_json["D_NEXT_O_ID"] = d_next_o_id;

    std::string d_tuple = d_tuple_json.dump();

    return d_tuple;
}

std::string TPCCData::GenerateCustomer(uint64_t c_id)
{
    std::string c_last;
    if (c_id <= 1000)
        c_last = UtilFunc::Lastname(c_id - 1);
    else
        c_last = UtilFunc::Lastname(UtilFunc::Rand(1, 999));
    std::string c_middle = "OE";
    std::string c_first = UtilFunc::MakeAlphaString(8, 16);
    std::string c_street = UtilFunc::MakeAlphaString(10, 20);
    std::string c_state = UtilFunc::MakeAlphaString(2, 2);
    std::string c_zip = UtilFunc::MakeNumberString(9, 9);
    std::string c_phone = UtilFunc::MakeNumberString(16, 16);
    uint64_t c_since = 0;
    std::string c_credit = "GC";
    uint64_t c_credit_lim = 50000;
    double c_discount = (double)UtilFunc::Rand(0L, 500L)/1000.0;
    double c_balance     = -10.0;
    double c_ytd_payment = 10.0;
    uint64_t c_payment_cnt = 1;
    uint64_t c_delivery_cnt = 0;
    std::string c_date = UtilFunc::MakeAlphaString(300, 500);

    json c_tuple_json;

    c_tuple_json["C_FIRST"] = c_first;
    c_tuple_json["C_MIDDLE"] = c_middle;
    c_tuple_json["C_LAST"] = c_last;
    c_tuple_json["C_STREET_1"] = c_street;
    c_tuple_json["C_STREET_2"] = c_street;
    c_tuple_json["C_CITY"] = c_street;
    c_tuple_json["C_STATE"] = c_state;
    c_tuple_json["C_ZIP"] = c_zip;
    c_tuple_json["C_PHONE"] = c_phone;
    c_tuple_json["C_SINCE"] = c_since;
    c_tuple_json["C_CREDIT"] = c_credit;
    c_tuple_json["C_CREDIT_LIM"] = c_credit_lim;
    c_tuple_json["C_DISCOUNT"] = c_discount;
    c_tuple_json["C_BALANCE"] = c_balance;
    c_tuple_json["C_YTD_PAYMENT"] = c_ytd_payment;
    c_tuple_json["C_PAYMENT_CNT"] = c_payment_cnt;
    c_tuple_json["C_DELIVERY_CNT"] = c_delivery_cnt;
    c_tuple_json["C_DATA"] = c_date;

    std::string c_tuple = c_tuple_json.dump();

    return c_tuple;
}

std::string TPCCData::GenerateStock()
{
    uint64_t s_quantity = UtilFunc::Rand(10, 100);
    std::string s_dist = UtilFunc::MakeAlphaString(24, 24);
    uint64_t s_ytd = 0;
    uint64_t s_order_cnt = 0;
    uint64_t s_remote_cnt = 0;
    std::string s_date = UtilFunc::MakeAlphaString(26, 50);

    json s_tuple_json;

    s_tuple_json["S_QUANTITY"] = s_quantity;
    s_tuple_json["S_DIST_01"] = s_dist;
    s_tuple_json["S_DIST_02"] = s_dist;
    s_tuple_json["S_DIST_03"] = s_dist;
    s_tuple_json["S_DIST_04"] = s_dist;
    s_tuple_json["S_DIST_05"] = s_dist;
    s_tuple_json["S_DIST_06"] = s_dist;
    s_tuple_json["S_DIST_07"] = s_dist;
    s_tuple_json["S_DIST_08"] = s_dist;
    s_tuple_json["S_DIST_09"] = s_dist;
    s_tuple_json["S_DIST_10"] = s_dist;
    s_tuple_json["S_YTD"] = s_ytd;
    s_tuple_json["S_ORDER_CNT"] = s_order_cnt;
    s_tuple_json["S_REMOTE_CNT"] = s_remote_cnt;
    s_tuple_json["S_DATA"] = s_date;

    std::string s_tuple = s_tuple_json.dump();

    return s_tuple;
}

int main()
{
    TPCCData tpccData(21,g_ware_num);

    tpccData.Generate();

    return 0;
}