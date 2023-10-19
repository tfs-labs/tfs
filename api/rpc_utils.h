/**
 * *****************************************************************************
 * @file        rpc_utils.h
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _TFSRPC_H
#define _TFSRPC_H
#include <exception>
#include <tuple>
#include <vector>
#include "utils/json.hpp"
#include "utils/tmp_log.h"

/**
 * @brief       Set the Error object
 * 
 * @param       error 
 */
void SetError(const std::string & error);

/**
 * @brief       Get the Error object
 * 
 * @return      std::string 
 */
std::string GetError();

/**
 * @brief       Set the No Error object
 * 
 */
void SetNoError();

/**
 * @brief       Set the Error Num object
 * 
 * @param       v 
 */
void SetErrorNum(int v);

/**
 * @brief       Set the No Error Num object
 * 
 */
void SetNoErrorNum();

/**
*/
int GetErrorNum();


/**
 * @brief       
 * 
 * @tparam N 
 * @tparam Ts 
 */
template<int N,typename ...Ts>
class GetField{
    public:
    GetField(std::tuple<Ts...> & ts,const nlohmann::json & obj)
    {
        try{
            std::get<N>(ts)=obj[N];
        }catch(std::exception & e){
            SetError(Sutil::Format("Field type error:%s [index:%s]", e.what(),N));
            return ;
        }
        GetField<N-1, Ts...>(ts,obj);
    }
   

};

/**
 * @brief       
 * 
 * @tparam Ts 
 */
template<typename ...Ts>
class GetField<0, Ts...>{
    public:
    GetField(std::tuple<Ts...> & ts,const nlohmann::json & obj){
        try{
             std::get<0>(ts)=obj[0];
        }catch(std::exception & e){
            SetError(Sutil::Format("Field type error:%s [index:%s]", e.what(),0));
        }
    }

};

/**
 * @brief       
 * 
 */
template<typename... Ts>
std::tuple<Ts...> GetRpcParams(const nlohmann::json & obj)
{
    std::tuple<Ts...> ret;
    if(obj.is_array()==false){
        SetError(Sutil::Format("json is not array:%s", obj.dump()));
        return ret;
    }
    GetField<sizeof...(Ts)-1, Ts...>(ret,obj);
    return ret;
}


/**
 * @brief       
 * 
 * @param       index 
 * @param       fields 
 * @param       obj 
 */
static void MakeField(int index,const std::vector<std::string> & fields,nlohmann::json & obj){

}

/**
 * @brief       
 * 
 * @tparam T 
 * @tparam Ts 
 * @param       index 
 * @param       fields 
 * @param       obj 
 * @param       value 
 * @param       ts 
 */
template <typename T,typename... Ts>
void MakeField(int index,const std::vector<std::string> & fields,nlohmann::json & obj,const T& value,const Ts &... ts){
    try{
        obj[fields[index]]=value;
    }catch(std::exception & e){
        SetError(Sutil::Format("Field type error:%s", e.what()));
        return ;
    }
    MakeField(index+1, fields, obj, ts...);
 }

/**
 * @brief       
 * 
 * @tparam T 
 * @param       index 
 * @param       fields 
 * @param       obj 
 * @param       value 
 */
template<typename T>
void  MakeField(int index,const std::vector<std::string> & fields,nlohmann::json & obj,const T &value){
    try{
        obj[fields[index]]=value;
    }catch(std::exception & e){
        SetError(Sutil::Format("Field type error:%s", e.what()));
        return ;
    }
}

/**
 * @brief       Create a Json Obj object
 * 
 * @tparam Ts 
 * @param       fields 
 * @param       values 
 * @return      nlohmann::json 
 */
template<typename... Ts>
nlohmann::json CreateJsonObj(const std::vector<std::string> & fields,const Ts &... values){
    nlohmann::json obj;
    MakeField(0,fields,obj,values...);
    return obj;
}

#endif