#include "./rpc_tx.h"
#include "utils/json.hpp"
#include "utils/tmp_log.h"


#define PARSE_PARAMS(value)\
  if(jsObject.contains(#value)){\
        if(jsObject[#value].is_string()) { \
              try { \
                jsParams = nlohmann::json::parse(jsObject[#value].get<std::string>()); \
              } catch (const nlohmann::json::parse_error& e) { \
                errorL("Parse error for key %s: %s", std::string(#value).c_str(), e.what()); \
                return Sutil::Format("parse fail for key %s: %s", std::string(#value).c_str(), e.what()); \
              } \
        } else if(jsObject[#value].is_object()) { \
              jsParams = jsObject[#value].get<nlohmann::json>(); \
            } else { \
              errorL("Unsupported type for key %s", std::string(#value).c_str()); \
              return Sutil::Format("unsupported type for key %s", std::string(#value).c_str()); \
            } \
  }else{\
            if(!jsParams.contains("isFindUtxo")){\
                jsParams[#value] = false;\
            }else if(!jsParams.contains("txInfo")){\
                jsParams[#value] = "";\
            }else{\
                errorL("not found key:%s" ,std::string(#value))                             \
            return Sutil::Format("not found key:%s",std::string(#value));\
            }\
  }       

#define PARSE_REGULAR_JSON(value)                                                      \
  if(jsObject.contains(#value)){\
		jsObject[#value].get_to(this->value);\
	}else{\
		if(!jsObject.contains("isFindUtxo")){\
			jsObject[#value] = false;\
		}else if(!jsObject.contains("txInfo")){\
			jsObject[#value] = "";\
		}else{\
			  errorL("not found key:%s" ,std::string(#value))                             \
        return Sutil::Format("not found key:%s",std::string(#value));\
		}\
	}

#define PARSE_JSON(value)\
  if(jsParams.contains(#value)){\
		jsParams[#value].get_to(this->value);\
	}else{\
		if(!jsParams.contains("isFindUtxo")){\
			jsParams[#value] = false;\
		}else if(!jsParams.contains("txInfo")){\
			jsParams[#value] = "";\
		}else{\
			  errorL("not found key:%s" ,std::string(#value))                             \
        return Sutil::Format("not found key:%s",std::string(#value));\
		}\
	}


#define TO_JSON(value) jsObject[#value] = this->value;
#define TO_RESULT(value) jsResult[#value] = this->value;
#define TO_PARAMS(value) jsParams[#value] = this->value;


#define PARSE_REQ(sname)                                                     \
  std::string sname::_paseFromJson(const std::string& json) {                  \
    nlohmann::json jsObject;                                                   \
    try {                                                                      \
        jsObject = nlohmann::json::parse(json);                                \
        nlohmann::json jsParams;\
        PARSE_REGULAR_JSON(id)\
        PARSE_REGULAR_JSON(jsonrpc)\
        PARSE_PARAMS(params)
        
#define PARSE_ACK(sname)                                                     \
  std::string sname::_paseFromJson(const std::string& json) {                   \
    nlohmann::json jsObject;                                                   \
    try {                                                                      \
        jsObject = nlohmann::json::parse(json);                                \
        nlohmann::json jsParams;\
        PARSE_REGULAR_JSON(id)\
        PARSE_REGULAR_JSON(method)\
        PARSE_REGULAR_JSON(jsonrpc)\
        PARSE_PARAMS(result)

#define DUMP_REQ(sname)                                                     \
  std::string sname::_paseToString() {                                          \
    nlohmann::json jsObject;                                                   \
    nlohmann::json jsParams;                                                   \
    try {                                                                      \
    TO_JSON(id)\
    TO_JSON(jsonrpc)\
    // TO_JSON(params)

#define DUMP_ACK(sname)                                                     \
  std::string sname::_paseToString() {                                          \
    nlohmann::json jsObject;                                                   \
    nlohmann::json jsResult;                                                   \
    try {                                                                      \
      TO_JSON(id)\
      TO_JSON(method)\
      TO_JSON(jsonrpc)
      // TO_JSON(result)

#define PARSE_END \
    }                                                                            \
    catch (std::exception & e) {                                                 \
        errorL("error:%s" , e.what());                                              \
        return e.what();                                                              \
    }                                                                            \
  return "OK";                                                                 \
  }

#define REQ_DUMP_END                                                                \
      jsObject["params"] = jsParams;\
      }                                                                            \
    catch (std::exception & e) {                                                 \
      errorL("error:%s",e.what());                                              \
      return std::string();                                                      \
    }                                                                            \
  return jsObject.dump();                                                      \
  }

#define ACK_DUMP_END                                                                \
        TO_RESULT(code)\
        TO_RESULT(message)\
        jsObject["result"] = jsResult;\
      }                                                                            \
    catch (std::exception & e) {                                                 \
      errorL("error:%s",e.what());                                              \
      return std::string();                                                      \
    }                                                                            \
  return jsObject.dump();                                                      \
}


PARSE_REQ(deploy_contract_req)
PARSE_JSON(addr)
PARSE_JSON(nContractType)
PARSE_JSON(info)
PARSE_JSON(contract)
PARSE_JSON(data)
PARSE_JSON(pubstr)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_END

DUMP_REQ(deploy_contract_req)
TO_PARAMS(addr)
TO_PARAMS(nContractType)
TO_PARAMS(info)
TO_PARAMS(contract)
TO_PARAMS(data)
TO_PARAMS(pubstr)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
REQ_DUMP_END


PARSE_REQ(call_contract_req)
PARSE_JSON(addr)
PARSE_JSON(deployer)
PARSE_JSON(deployutxo)
PARSE_JSON(args)
PARSE_JSON(pubstr)
PARSE_JSON(tip)
PARSE_JSON(money)
PARSE_JSON(istochain)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_JSON(contractAddress)
PARSE_END

DUMP_REQ(call_contract_req)
TO_PARAMS(addr)
TO_PARAMS(deployer)
TO_PARAMS(deployutxo)
TO_PARAMS(args)
TO_PARAMS(pubstr)
TO_PARAMS(tip)
TO_PARAMS(money)
TO_PARAMS(istochain)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
TO_PARAMS(contractAddress)
REQ_DUMP_END


PARSE_REQ(tx_req)
PARSE_JSON(fromAddr)
if (jsParams.contains("toAddr")) {
  auto map_ = jsParams["toAddr"];

  for (auto iter = map_.begin(); iter != map_.end(); iter++) {
    auto obj_c = iter.value();
    std::string addr_t;
    std::string value_t;
    if (obj_c.contains("addr")) {
      obj_c["addr"].get_to(addr_t);
    } else {
      errorL("not found key addr");
    }
    if (obj_c.contains("value")) {
      obj_c["value"].get_to(value_t);
    } else {
      errorL("not found key value");
    }
    toAddr[addr_t] = value_t;
  }
} else {
  errorL("not found key toAddr");
}
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_END

DUMP_REQ(tx_req)
TO_PARAMS(fromAddr)
nlohmann::json to_addrs;
for (auto iter = toAddr.begin(); iter != toAddr.end(); iter++) {
  nlohmann::json to_addr;
  to_addr["addr"] = iter->first;
  to_addr["value"] = iter->second;
  to_addrs.push_back(to_addr);
}
jsParams["toAddr"] = to_addrs;
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
REQ_DUMP_END


PARSE_REQ(getStakeReq)
PARSE_JSON(fromAddr)
PARSE_JSON(stakeAmount)
PARSE_JSON(PledgeType)
PARSE_JSON(commissionRate)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_END

DUMP_REQ(getStakeReq)
TO_PARAMS(fromAddr)
TO_PARAMS(stakeAmount)
TO_PARAMS(PledgeType)
TO_PARAMS(commissionRate)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
REQ_DUMP_END


PARSE_REQ(getUnStakeReq)
PARSE_JSON(fromAddr)
PARSE_JSON(utxoHash)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_END

DUMP_REQ(getUnStakeReq)
TO_PARAMS(fromAddr)
TO_PARAMS(utxoHash)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
REQ_DUMP_END


PARSE_REQ(getInvestReq)
PARSE_JSON(fromAddr)
PARSE_JSON(toAddr)
PARSE_JSON(investAmount)
PARSE_JSON(investType)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_END

DUMP_REQ(getInvestReq)
TO_PARAMS(fromAddr)
TO_PARAMS(toAddr)
TO_PARAMS(investAmount)
TO_PARAMS(investType)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
REQ_DUMP_END


PARSE_REQ(getDisinvestreq)
PARSE_JSON(fromAddr)
PARSE_JSON(toAddr)
PARSE_JSON(utxoHash)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_END

DUMP_REQ(getDisinvestreq)
TO_PARAMS(fromAddr)
TO_PARAMS(toAddr)
TO_PARAMS(utxoHash)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
REQ_DUMP_END


PARSE_REQ(getBonusReq)
PARSE_JSON(Addr)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_END

DUMP_REQ(getBonusReq)
TO_PARAMS(Addr)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
REQ_DUMP_END

PARSE_REQ(GetRatesInfoReq)
PARSE_END

DUMP_REQ(GetRatesInfoReq)
REQ_DUMP_END

PARSE_ACK(GetRatesInfoAck)
PARSE_JSON(TotalCirculatingSupply)
PARSE_JSON(TotalBurn)
PARSE_JSON(TotalStaked)
PARSE_JSON(StakingRate)
PARSE_JSON(CurrentAPR)
PARSE_END

DUMP_ACK(GetRatesInfoAck)
TO_RESULT(TotalCirculatingSupply)
TO_RESULT(TotalBurn)
TO_RESULT(TotalStaked)
TO_RESULT(StakingRate)
TO_RESULT(CurrentAPR)
ACK_DUMP_END


PARSE_REQ(get_stakeutxo_req)
PARSE_JSON(fromAddr)
PARSE_END

DUMP_REQ(get_stakeutxo_req)
TO_PARAMS(fromAddr)
REQ_DUMP_END


DUMP_ACK(get_stakeutxo_ack)
TO_RESULT(utxos)
ACK_DUMP_END


PARSE_REQ(get_disinvestutxo_req)
PARSE_JSON(fromAddr)
PARSE_JSON(toAddr)
PARSE_END

DUMP_REQ(get_disinvestutxo_req)
TO_PARAMS(fromAddr)
TO_PARAMS(toAddr)
REQ_DUMP_END

DUMP_ACK(get_disinvestutxo_ack)
TO_RESULT(utxos)
ACK_DUMP_END


PARSE_REQ(confirm_transaction_req)
PARSE_JSON(txhash)
PARSE_JSON(height)
PARSE_END

DUMP_REQ(confirm_transaction_req)
TO_PARAMS(txhash)
TO_PARAMS(height)
REQ_DUMP_END

DUMP_ACK(confirm_transaction_ack)
TO_RESULT(txhash)
TO_RESULT(percent)
TO_RESULT(sendsize)
TO_RESULT(receivedsize)
TO_RESULT(tx)
ACK_DUMP_END


PARSE_REQ(get_tx_info_req)
PARSE_JSON(txhash)
PARSE_END

DUMP_REQ(get_tx_info_req)
TO_JSON(txhash)
REQ_DUMP_END

PARSE_ACK(get_tx_info_ack)
PARSE_JSON(tx)
PARSE_JSON(blockhash)
PARSE_JSON(blockheight)
PARSE_END

DUMP_ACK(get_tx_info_ack)
TO_RESULT(tx)
TO_RESULT(blockhash)
TO_RESULT(blockheight)
ACK_DUMP_END

PARSE_REQ(getAllbonusInfoReq)
PARSE_END

DUMP_REQ(getAllbonusInfoReq)
REQ_DUMP_END

DUMP_ACK(getAllbonusInfoAck)
TO_RESULT(info)
ACK_DUMP_END

PARSE_REQ(get_all_stake_node_list_req)
PARSE_END

DUMP_REQ(get_all_stake_node_list_req)
REQ_DUMP_END

PARSE_ACK(get_all_stake_node_list_ack)
PARSE_END

DUMP_ACK(get_all_stake_node_list_ack)
TO_RESULT(list)
ACK_DUMP_END


PARSE_REQ(getblocknumberReq)
PARSE_END

DUMP_REQ(getblocknumberReq)
REQ_DUMP_END


PARSE_ACK(getblocknumberAck)
PARSE_JSON(top)
PARSE_END

DUMP_ACK(getblocknumberAck)
TO_RESULT(top)
ACK_DUMP_END


PARSE_REQ(getversionReq)
PARSE_END

DUMP_REQ(getversionReq)
REQ_DUMP_END

PARSE_ACK(getversionAck)
PARSE_JSON(netVersion)
PARSE_JSON(clientVersion)
PARSE_JSON(configVersion)
PARSE_JSON(dbVersion)
PARSE_END

DUMP_ACK(getversionAck)
TO_RESULT(netVersion)
TO_RESULT(clientVersion)
TO_RESULT(configVersion)
TO_RESULT(dbVersion)
ACK_DUMP_END


PARSE_REQ(balanceReq)
PARSE_JSON(addr)
PARSE_END

DUMP_REQ(balanceReq)
TO_PARAMS(addr)
REQ_DUMP_END

PARSE_ACK(balanceAck)
PARSE_JSON(balance)
PARSE_JSON(addr)
PARSE_END

DUMP_ACK(balanceAck)
TO_RESULT(balance)
TO_RESULT(addr)
ACK_DUMP_END


PARSE_REQ(getblocktransactioncountReq)
PARSE_JSON(blockHash)
PARSE_END

DUMP_REQ(getblocktransactioncountReq)
TO_PARAMS(blockHash)
REQ_DUMP_END

PARSE_ACK(getblocktransactioncountAck)
PARSE_JSON(txCount)
PARSE_END

DUMP_ACK(getblocktransactioncountAck)
TO_RESULT(txCount)
ACK_DUMP_END


PARSE_REQ(getaccountsReq)
PARSE_END

DUMP_REQ(getaccountsReq)
REQ_DUMP_END

PARSE_ACK(getaccountsAck)
PARSE_JSON(acccountlist)
PARSE_END

DUMP_ACK(getaccountsAck)
TO_RESULT(acccountlist)
ACK_DUMP_END


PARSE_REQ(getchainidReq)
PARSE_END

DUMP_REQ(getchainidReq)
REQ_DUMP_END

PARSE_ACK(getchainidAck)
PARSE_JSON(chainId)
PARSE_END

DUMP_ACK(getchainidAck)
TO_RESULT(chainId)
ACK_DUMP_END


PARSE_REQ(getpeerlistReq)
PARSE_END

DUMP_REQ(getpeerlistReq)
REQ_DUMP_END

DUMP_ACK(getpeerlistAck)
TO_RESULT(peers)
TO_RESULT(size)
ACK_DUMP_END


PARSE_REQ(getTransactionInfoReq)
PARSE_JSON(txHash)
PARSE_END

DUMP_REQ(getTransactionInfoReq)
TO_JSON(txHash)
REQ_DUMP_END

DUMP_ACK(getTransactionInfoAck)
TO_RESULT(tx)
ACK_DUMP_END


PARSE_REQ(getBlockInfoByHashReq)
PARSE_JSON(blockHash)
PARSE_END

DUMP_REQ(getBlockInfoByHashReq)
TO_JSON(blockHash)
REQ_DUMP_END

DUMP_ACK(getBlockInfoByHashAck)
TO_RESULT(blockInfo)
ACK_DUMP_END

PARSE_REQ(getBlockInfoByHeightReq)
PARSE_JSON(beginHeight)
PARSE_JSON(endHeight)
PARSE_END

DUMP_REQ(getBlockInfoByHeightReq)
TO_JSON(beginHeight)
TO_JSON(endHeight)
REQ_DUMP_END

DUMP_ACK(getBlockInfoByHeightAck)
TO_RESULT(blocks)
ACK_DUMP_END

PARSE_ACK(rpcAck)
PARSE_JSON(txHash)
PARSE_END

DUMP_ACK(rpcAck)
TO_RESULT(txHash)
ACK_DUMP_END

PARSE_ACK(txAck)
PARSE_JSON(txJson)
PARSE_JSON(height)
PARSE_JSON(vrfJson)
PARSE_JSON(txType)
PARSE_JSON(time)
PARSE_JSON(gas)
PARSE_END

DUMP_ACK(txAck)
TO_RESULT(txJson)
TO_RESULT(height)
TO_RESULT(vrfJson)
TO_RESULT(txType)
TO_RESULT(time)
TO_RESULT(gas)
ACK_DUMP_END


PARSE_ACK(contractAck)
PARSE_JSON(contractJs)
PARSE_JSON(txJson)
PARSE_END


DUMP_ACK(contractAck)
TO_RESULT(contractJs)
TO_RESULT(txJson)
ACK_DUMP_END


PARSE_REQ(GetDelegateReq)
PARSE_JSON(addr)
PARSE_END

DUMP_REQ(GetDelegateReq)
TO_JSON(addr)
REQ_DUMP_END

PARSE_ACK(GetDelegateAck)
PARSE_JSON(info)
PARSE_END

DUMP_ACK(GetDelegateAck)
TO_RESULT(info)
ACK_DUMP_END