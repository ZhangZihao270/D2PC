#ifndef   UTIL_UTIL_FUNCTION_H_
#define   UTIL_UTIL_FUNCTION_H_

// #include "config.h"
#include <random>
#include "string.h"

using namespace std;

class UtilFunc
{

    // /* 
    //  * 随机数种子
    //  * 每个线程拥有独立的 rand_buffer，根据线程ID映射一对一映射，线程间不存在竞争
    //  */
    // static struct drand48_data rand_buffers_[MAX_THREAD_NUM];

    // /* 
    //  * TPCC负载有关的变量
    //  */
    /** NURand函数中的常量C **/
    // static uint64_t C_C_LAST_255 , C_C_ID_1023, C_I_ID_8191;
    
    /** 包含 1-9 a-z A-Z 共61个字符，用于生成字符串 **/
    static const char char_list_[61];
    /** 包含 10 个单词，用于生成customer的last name **/
    static const std::string last_name_list_[10];


public:
    //初始化TPCC中NURand函数使用的常量C
        // UtilFunc::C_C_LAST_255 = (uint64_t )UtilFunc::Rand(0, 255);
        // UtilFunc::C_C_ID_1023  = (uint64_t )UtilFunc::Rand(0, 1023);
        // UtilFunc::C_I_ID_8191  = (uint64_t )UtilFunc::Rand(0, 8191);
    

    /*
     * tpcc负载中使用的NURand函数
     * @param A: 255(C_LAST) 1023(C_ID) 8191(OL_I_ID)
     */
    // static uint64_t NURand(uint64_t  A, uint64_t x, uint64_t y)
    // {
    //     uint64_t C = 0;
    //     switch(A) {
    //         case 255:
    //             C = C_C_LAST_255;
    //             break;
    //         case 1023:
    //             C = C_C_ID_1023;
    //             break;
    //         case 8191:
    //             C = C_I_ID_8191;
    //             break;
    //         default:
    //             printf("NURand函数接收到无法识别的随机常数!\n");
    //             break;
    //     }
    //     return(((UtilFunc::Rand(0, A) | UtilFunc::Rand(x, y)) + C) % (y - x + 1)) + x;
    // }


    static uint64_t Rand(int min, int max)
    {
        std::random_device rd;
        std::mt19937 gen(rd());

        std::uniform_int_distribution<> distrib(min, max);

        int random_number = distrib(gen);
        return random_number;
    }
    
    /*
     * 生成长度在[min, max]之间的字符串，字符串可包括数字1-9、大小写字母
     * 注意，传入的str必须已经申请足够的空间，函数只负责填充，不负责申请空间
     */
    static std::string MakeAlphaString(uint64_t min, uint64_t max)
    {
        uint64_t length = UtilFunc::Rand(min, max);
        std::string str = "";
        for (uint32_t i = 0; i < length; i++) 
            str += char_list_[UtilFunc::Rand(0L, 60L)];
        return str;
    }

    /*
     * 同上，只是生成的字符串只包括数字0-9
     */
    static std::string MakeNumberString(uint64_t min, uint64_t max)
    {
        uint64_t length = UtilFunc::Rand(min, max);
        std::string str = "";
        for (uint32_t i = 0; i < length; i++) {
            uint64_t r = UtilFunc::Rand(0L,9L);
            str += '0' + r;
        }
        return str;
    }

    /** 生成customer的last name
     *  传入0-999范围内的整数 **/
    static std::string Lastname(uint64_t  num)
    {
        std::string str = "";
        str += last_name_list_[num/100];
        str += last_name_list_[(num/10)%10];
        str += last_name_list_[num%10];

        return str;
    }

};

const char UtilFunc::char_list_[61] = {'1','2','3','4','5','6','7','8','9','a','b','c',
                        'd','e','f','g','h','i','j','k','l','m','n','o',
                        'p','q','r','s','t','u','v','w','x','y','z','A',
                        'B','C','D','E','F','G','H','I','J','K','L','M',
                        'N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};

const std::string UtilFunc::last_name_list_[10] = {"BAR", "OUGHT", "ABLE", "PRI", "PRES",
                                "ESE", "ANTI", "CALLY", "ATION", "EING"};

    // static void InitUtilFunc()
    // {
        // //初始化随机数种子
        // for ( uint64_t i = 0; i < MAX_THREAD_NUM; i++)
        // {
        //     srand48_r(i+1, &UtilFunc::rand_buffers_[i]);
        // }
        
        //初始化TPCC中NURand函数使用的常量C
        // UtilFunc::C_C_LAST_255 = UtilFunc::Rand(0, 255);
        // UtilFunc::C_C_ID_1023  = UtilFunc::Rand(0, 1023);
        // UtilFunc::C_I_ID_8191  = UtilFunc::Rand(0, 8191);

    // }

#endif