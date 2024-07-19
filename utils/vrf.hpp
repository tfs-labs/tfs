#ifndef _VRF_H_
#define _VRF_H_

#include "account_manager.h"
#include "utils/timer.hpp"
#include "utils/tmp_log.h"
#include <cstdint>
#include <shared_mutex>

constexpr int cacheExpireTime = 300000000;

class VRF
{
    public:
        VRF() {
            ClearTimer.AsyncLoop(3000, [&]() {
                 uint64_t time_ = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
                {
                    std::unique_lock<std::shared_mutex> lck(vrfInfoMutex);
                   
                    auto v1Iter = vrfCache.begin();
                   
                    
                    for (; v1Iter != vrfCache.end();) {
                        if ((time_ - v1Iter->second.second) > cacheExpireTime) {
                            DEBUGLOG("vrfCache erase hash : {}  time", v1Iter->first, v1Iter->second.second);
                            v1Iter= vrfCache.erase(v1Iter);
                        } else {
                            v1Iter++;
                        }
                    }
                    auto v2Iter = txvrfCache.begin();
                    for (; v2Iter != txvrfCache.end();) {
                        if ((time_ - v2Iter->second.second) > cacheExpireTime) {
                           DEBUGLOG("txvrfCache erase hash : {}  time", v2Iter->first, v2Iter->second.second);
                           v2Iter= txvrfCache.erase(v2Iter);
                        } else {
                            v2Iter++;
                        }
                    }
                }

                {
                    std::unique_lock<std::shared_mutex> lck(vrfNodeMutex);
                    auto v3Iter = vrfVerifyNode.begin();
                    for(;v3Iter!=vrfVerifyNode.end();){
                    if((time_ - v3Iter->second.second ) > cacheExpireTime){
                        DEBUGLOG("txvrfCache erase hash : {}  time", v3Iter->first, v3Iter->second.second);
                        v3Iter= vrfVerifyNode.erase(v3Iter);
                    }else{
                       v3Iter++;
                    }
                }
                }
                
            });
        }
        ~VRF() = default;

        /**
         * @brief       
         * 
         * @param       pkey:
         * @param       input:
         * @param       output:
         * @param       proof:
         * @return      int
        */
        int CreateVRF(EVP_PKEY* pkey, const std::string& input, std::string & output, std::string & proof)
        {
            std::string hash = Getsha256hash(input);
	        if(ED25519SignMessage(hash, pkey, proof) == false)
            {
                return -1;
            }

            output = Getsha256hash(proof);
            return 0;
        }
        /**
         * @brief       
         * 
         * @param       pkey:
         * @param       input:
         * @param       output:
         * @param       proof:
         * @return      int
        */
        int VerifyVRF(EVP_PKEY* pkey, const std::string& input, std::string & output, std::string & proof)
        {
            std::string hash = Getsha256hash(input);
            if(ED25519VerifyMessage(hash, pkey, proof) == false)
            {
                return -1;
            }

            output = Getsha256hash(proof);
            return 0;
        }
        /**
         * @brief       
         *
         * @return      int
        */
        int testVRF()
        {
            Account account(true);
            auto pkey=account.GetKey();

            std::string test="hello,world!";
            std::string output,proof;
            int ret = CreateVRF(pkey, test, output, proof);
            if(ret != 0){
                std::cout << "error create:" << ret << std::endl;
                return -2;
            }

            ret = VerifyVRF(pkey, test, output, proof);
            if(ret != 0){
                std::cout << "error verify: " << ret << std::endl;
                return -1;
            }

            return 0;
            
        }
        /**
         * @brief       
         * 
         * @param       data:
         * @param       limit:
         * @return      int
        */
        int GetRandNum(std::string data, uint32_t limit)
        {
            auto value = stringToll(data);
            return  value % limit;
        }
        /**
         * @brief       
         * 
         * @param       data:
         * @return      double
        */
        double GetRandNum(const std::string& data)
        {
            auto value = stringToll(data);
            return  double(value % 100) / 100.0;
        }
        /**
         * @brief       
         * 
         * @param       TxHash:
         * @param       info:
        */
        void addVrfInfo(const std::string & TxHash,Vrf & info){
            
            std::unique_lock<std::shared_mutex> lck(vrfInfoMutex);
            uint64_t time_= MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
            vrfCache[TxHash]={info,time_};
        }
        /**
         * @brief       
         * 
         * @param       TxHash:
         * @param       info:
        */
        void addTxVrfInfo(const std::string & TxHash,const Vrf & info){
            std::unique_lock<std::shared_mutex> lck(vrfInfoMutex);
            uint64_t time_= MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
            txvrfCache[TxHash]={info,time_};
        }
        /**
         * @brief       
         * 
         * @param       TxHash:
         * @param       vrf:
         * @return      true
         * @return      false
        */
        bool getVrfInfo(const std::string & TxHash,std::pair<std::string,Vrf> & vrf){
            std::shared_lock<std::shared_mutex> lck(vrfInfoMutex);
            auto ite= vrfCache.find(TxHash);
            if(ite!=vrfCache.end()){
                vrf.first=ite->first;
                vrf.second=ite->second.first;
                return true;
            }
            return false;
        }
        /**
         * @brief       
         * 
         * @param       TxHash:
         * @param       vrf:
         * @return      true
         * @return      false
        */
        bool getTxVrfInfo(const std::string & TxHash,std::pair<std::string,Vrf> & vrf){
            std::shared_lock<std::shared_mutex> lck(vrfInfoMutex);
            auto ite= txvrfCache.find(TxHash);
            if(ite!=txvrfCache.end()){
                vrf.first=ite->first;
                vrf.second=ite->second.first;
                return true;
            }
            return false;
        }
        /**
        * @brief       
        * 
        * @param       TxHash:
        * @param       AddressVector:
        */
        void addVerifyNodes(const std::string & TxHash,std::vector<std::string> & AddressVector){
            std::unique_lock<std::shared_mutex> lck(vrfNodeMutex);
            uint64_t time_= MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
            vrfVerifyNode[TxHash]={AddressVector, time_};
        }
        /**
        * @brief       
        * 
        * @param       TxHash:
        * @param       nodesPair:
        * @return      true
        * @return      false
        */
        bool  getVerifyNodes(const std::string & TxHash,std::pair<std::string,std::vector<std::string>> & nodesPair){
            std::shared_lock<std::shared_mutex> lck(vrfNodeMutex);
            auto ite= vrfVerifyNode.find(TxHash);
            if(ite!=vrfVerifyNode.end()){
                nodesPair.first=ite->first;
                nodesPair.second=ite->second.first;
                return true;
            }
            return false;
        }

        void StopTimer() { ClearTimer.Cancel(); }
     private:
        
        friend std::string PrintCache(int where);
        std::map<std::string,std::pair<Vrf,uint64_t>> vrfCache; //Block flow vrf
        std::map<std::string,std::pair<Vrf,uint64_t>> txvrfCache; //Vrf of transaction flows
        std::map<std::string,std::pair<std::vector<std::string>,uint64_t>> vrfVerifyNode; //The vrf corresponds to the verification address required by the hash
        std::shared_mutex vrfInfoMutex;
        std::shared_mutex vrfNodeMutex;
        CTimer ClearTimer;
        /**
        * @brief       
        * 
        * @param       data:
        * @return       long long
        */
        long long stringToll(const std::string& data)
        {
            long long value = 0;
            for(int i = 0;i< data.size() ;i++)
            {
                    int a= (int )data[i];
                    value += a;
            }
            return value;
        }
};






#endif