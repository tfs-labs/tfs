#include <db_api.h>
#include "ca/ca.h"
#include "ca/contract.h"
#include "ca/tfs_wasmtime.h"
#include "transaction.h"

thread_local wasmtime::_Error _host_error;

void TFSC::WasmtimeVMhost::SetHostError(const std::string & er,int errenum){
    _host_error.er=er;
    _host_error.er_n=errenum;
}

wasmtime::_Error TFSC::WasmtimeVMhost::GetHostError(){
    wasmtime::_Error ret=_host_error;
    _host_error.er_n=0;
    return ret;
}


void TFSC::_wasm_time_init(){
    std::shared_ptr<TFSC::WasmtimeVMhost> hostFunctions =  MagicSingleton<TFSC::WasmtimeVMhost>::GetInstance();
    TFSC::MakeHost<std::string ,std::string>(*hostFunctions,"GetBalance",[](const std::string &fromAddr )->std::string
    {
        uint64_t balance = 0;
        GetBalanceByUtxo(fromAddr, balance);
        return std::to_string(balance);
    });

    TFSC::MakeHost<void, std::string,std::string>
    (*hostFunctions,"Transfer",[](const std::string &to,const std::string &value){
        transation_target iter;
        std::string contractAddr = MagicSingleton<TFSC::StoreManager>::GetInstance()->getCurrentContractAddr();
        iter.from = contractAddr;
        iter.to = to;
        iter.amount = value;
        MagicSingleton<TFSC::StoreManager>::GetInstance()->InstertTransationTarget(contractAddr, iter);
       
    });

    TFSC::MakeHost<void ,std::string,std::string>(*hostFunctions,"SetStore",[](const std::string & key,const std::string & value){
        std::string contractAddr = MagicSingleton<TFSC::StoreManager>::GetInstance()->getCurrentContractAddr();
        TFSC::WasmStore::ptr iter = MagicSingleton<TFSC::StoreManager>::GetInstance()->GetStore(contractAddr);
        iter->InsterValue(key, value);
    });

    TFSC::MakeHost<std::string, std::string>(*hostFunctions,"GetStore",[](const std::string & str)->std::string
    {
        auto p=  MagicSingleton<TFSC::StoreManager>::GetInstance();
        TFSC::WasmStore::ptr ta=MagicSingleton<TFSC::StoreManager>::GetInstance()->GetStore(p->getCurrentContractAddr());
        return ta->GetValue(str);
    });

    TFSC::MakeHost<long long>(*hostFunctions,"GetTime",[]()->long long
    {
        std::cout << "input time:" << std::endl;
        long long t=100;
        return t;
    });

  
    
    TFSC::MakeHost<void ,std::string,std::string>(*hostFunctions,"Selfdestruct",[&](const std::string & destoryAddr, const std::string & receiveAddr){

        std::string strPrevTxHash;
        DBReader dataReader;
        auto ret = dataReader.GetLatestUtxoByContractAddr(destoryAddr, strPrevTxHash);
        if(ret != DBStatus::DB_SUCCESS)
        {
            std::cout << "GetLatestUtxoByContractAddr fail" << std::endl;
        }

        CTransaction PrevTx;
        std::string transactionRaw;
        ret = dataReader.GetTransactionByHash(strPrevTxHash, transactionRaw);

        if(ret != DBStatus::DB_SUCCESS)    
        {
            hostFunctions->SetHostError("GetTransactionByHash fail", -1);
            std::cout << " GetTransactionByHash fail" << std::endl;
            return;
        }

        if(!PrevTx.ParseFromString(transactionRaw))
        {
            hostFunctions->SetHostError("ParseFromString tx  fail", -2);
            std::cout << "ParseFromString tx  fail" << std::endl;
            return;
        }
        
        std::string contractDeployAddr = PrevTx.utxo().owner(0);
        std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
        if(contractDeployAddr != defaultAddr)
        {
            hostFunctions->SetHostError("The contract destruction error is executed, and the current account is not the contract owner", -3);
            std::cout << "The contract destruction error is executed, and the current account is not the contract owner" << std::endl;
            return;
        }


        destruction_information iter;
        iter.destoryAddr = destoryAddr;
        iter.receiveAddr = receiveAddr;
    
        uint64_t balance = 0;
        if(GetBalanceByUtxo(destoryAddr, balance) != 0)
        {
            balance = 0;
        }

        iter.amount = std::to_string(balance);
        MagicSingleton<TFSC::StoreManager>::GetInstance()->destruction_data.push_back(iter);
    });
    
}