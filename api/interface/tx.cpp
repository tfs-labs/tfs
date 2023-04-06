#include "tx.h"
#include "utils/tmplog.h"
#include "utils/json.hpp"
#define PARSE_JSON(value)\
	if(jsObject.contains(#value)){\
		jsObject[#value].get_to(this->value);\
	}else{\
		errorL("not found key:"+std::string(#value))\
	}

#define PARSE_JSON_OBJ(obj,target,value)\
	if(obj.contains(#target)){\
		obj[#target].get_to(value);\
	}else{\
		errorL("not found key:"+std::string(#target))\
	}


#define TO_JSON_OBJ(obj,target,value)\
		obj[#target]=value;

#define TO_JSON(value)\
	jsObject[#value] = this->value;



#define PAUSE_D_ACK(sname) bool sname::paseFromJson(std::string json){\
	nlohmann::json jsObject;\
	try{\
	jsObject = nlohmann::json::parse(json);\
	PARSE_JSON(type)\
	PARSE_JSON(ErrorCode)\
	PARSE_JSON(ErrorMessage)


#define PAUSE_D_REQ(sname) bool sname::paseFromJson(std::string json){\
	nlohmann::json jsObject;\
	try{\
	jsObject = nlohmann::json::parse(json);\
	PARSE_JSON(type)

#define DOUMP_D_ACK(sname) std::string sname::paseToString(){\
	nlohmann::json jsObject;\
	try{\
	TO_JSON(type)\
	TO_JSON(ErrorCode)\
	TO_JSON(ErrorMessage)

#define DOUMP_D_REQ(sname) std::string sname::paseToString(){\
	nlohmann::json jsObject;\
	try{\
	TO_JSON(type)

	
#define D_END\
	}catch(std::exception & e){\
		errorL("error:" <<e.what() );\
		return false;\
	}\
	return true;\
	}
#define D_END_R\
	}catch(std::exception & e){\
		errorL("error:" <<e.what() );\
		return std::string();\
	}\
	return jsObject.dump();\
}




//the_top
PAUSE_D_ACK(the_top)
PARSE_JSON(top)
D_END

DOUMP_D_REQ(the_top)
TO_JSON(top)

D_END_R


PAUSE_D_REQ(getsigvalue_req)
PARSE_JSON(addr)
PARSE_JSON(message)
D_END


DOUMP_D_REQ(getsigvalue_req)
TO_JSON(addr)
TO_JSON(message)
D_END_R


PAUSE_D_ACK(getsigvalue_ack)
PARSE_JSON(signature)
PARSE_JSON(pub)
D_END

DOUMP_D_ACK(getsigvalue_ack)
TO_JSON(signature)
TO_JSON(pub)
D_END_R




PAUSE_D_REQ(balance_req)

PARSE_JSON(addr)

D_END

DOUMP_D_REQ(balance_req)
TO_JSON(addr)
D_END_R

PAUSE_D_ACK(balance_ack)
PARSE_JSON(addr)
PARSE_JSON(balance)

D_END

DOUMP_D_ACK(balance_ack)
TO_JSON(addr)
TO_JSON(balance)
D_END_R



PAUSE_D_REQ(contract_info)
PARSE_JSON(name)
PARSE_JSON(language)
PARSE_JSON(languageVersion)
PARSE_JSON(standard)
PARSE_JSON(logo)
PARSE_JSON(source)
PARSE_JSON(ABI)
PARSE_JSON(userDoc)
PARSE_JSON(developerDoc)
PARSE_JSON(compilerVersion)
PARSE_JSON(compilerOptions)
PARSE_JSON(srcMap)
PARSE_JSON(srcMapRuntime)
PARSE_JSON(metadata)
PARSE_JSON(other)
D_END

DOUMP_D_REQ(contract_info)
TO_JSON(name)
TO_JSON(language)
TO_JSON(languageVersion)
TO_JSON(standard)
TO_JSON(logo)
TO_JSON(source)
TO_JSON(ABI)
TO_JSON(userDoc)
TO_JSON(developerDoc)
TO_JSON(compilerVersion)
TO_JSON(compilerOptions)
TO_JSON(srcMap)
TO_JSON(srcMapRuntime)
TO_JSON(metadata)
TO_JSON(other)
D_END_R



PAUSE_D_REQ(deploy_contract_req)
PARSE_JSON(addr)
PARSE_JSON(nContractType)
PARSE_JSON(info)
PARSE_JSON(contract)
PARSE_JSON(data)
PARSE_JSON(pubstr)
D_END


DOUMP_D_REQ(deploy_contract_req)
TO_JSON(addr)
TO_JSON(nContractType)
TO_JSON(info)
TO_JSON(contract)
TO_JSON(data)
TO_JSON(pubstr)
D_END_R




PAUSE_D_REQ(call_contract_req)
PARSE_JSON(addr)
PARSE_JSON(deployer)
PARSE_JSON(deployutxo)
PARSE_JSON(args)
PARSE_JSON(pubstr)
D_END


DOUMP_D_REQ(call_contract_req)
TO_JSON(addr)
TO_JSON(deployer)
TO_JSON(deployutxo)
TO_JSON(args)
TO_JSON(pubstr)
D_END_R

PAUSE_D_REQ(deploy_utxo_req)
PARSE_JSON(addr)
D_END


DOUMP_D_REQ(deploy_utxo_req)
TO_JSON(addr)
D_END_R


PAUSE_D_ACK(deploy_utxo_ack)
PARSE_JSON(utxos)
D_END


DOUMP_D_ACK(deploy_utxo_ack)
TO_JSON(utxos)
D_END_R



PAUSE_D_ACK(deployers_ack)
PARSE_JSON(deployers)
D_END

DOUMP_D_REQ(deployers_ack)
TO_JSON(deployers)
D_END_R




PAUSE_D_REQ(tx_req)
PARSE_JSON(fromAddr)
if (jsObject.contains("toAddr")) {
	auto map_ = jsObject["toAddr"];
	
	for (auto iter = map_.begin(); iter != map_.end(); iter++) {
		auto obj_c = iter.value();
		std::string addr_t;
		std::string value_t;
		if (obj_c.contains("addr")) {
			obj_c["addr"].get_to(addr_t);
		}
		else {
			errorL("not found key addr");
		}
		if (obj_c.contains("value")) {
			obj_c["value"].get_to(value_t);
		}
		else {
			errorL("not found key value");
		}
		toAddr[addr_t] = value_t;
	}
}
else {
	errorL("not found key toAddr");
}

D_END


DOUMP_D_REQ(tx_req)
TO_JSON(fromAddr)
nlohmann::json to_addrs;
for (auto iter = toAddr.begin(); iter != toAddr.end(); iter++) {
	nlohmann::json to_addr;
	to_addr["addr"] = iter->first;
	to_addr["value"] = iter->second;
	to_addrs.push_back(to_addr);
}
jsObject["toAddr"] = to_addrs;
D_END_R


PAUSE_D_REQ(getStake_req)
PARSE_JSON(fromAddr)
PARSE_JSON(stake_amount)
PARSE_JSON(PledgeType)
D_END

DOUMP_D_REQ(getStake_req)
TO_JSON(fromAddr)
TO_JSON(stake_amount)
TO_JSON(PledgeType)
D_END_R


PAUSE_D_REQ(getUnstake_req)
PARSE_JSON(fromAddr)
PARSE_JSON(utxo_hash)
D_END
DOUMP_D_REQ(getUnstake_req)
TO_JSON(fromAddr)
TO_JSON(utxo_hash)
D_END_R


PAUSE_D_REQ(getInvest_req)
PARSE_JSON(fromAddr)
PARSE_JSON(toAddr)
PARSE_JSON(invest_amount)
PARSE_JSON(investType)
D_END
DOUMP_D_REQ(getInvest_req)
TO_JSON(fromAddr)
TO_JSON(toAddr)
TO_JSON(invest_amount)
TO_JSON(investType)
D_END_R


PAUSE_D_REQ(getDisInvest_req)
PARSE_JSON(fromAddr)
PARSE_JSON(toAddr)
PARSE_JSON(utxo_hash)
D_END
DOUMP_D_REQ(getDisInvest_req)
TO_JSON(fromAddr)
TO_JSON(toAddr)
TO_JSON(utxo_hash)
D_END_R


PAUSE_D_REQ(getDeclare_req)
PARSE_JSON(fromAddr);
PARSE_JSON(toAddr);
PARSE_JSON(amount);
PARSE_JSON(multiSignPub);
PARSE_JSON(signAddrList);
PARSE_JSON(signThreshold);
D_END
DOUMP_D_REQ(getDeclare_req)
TO_JSON(fromAddr);
TO_JSON(toAddr);
TO_JSON(amount);
TO_JSON(multiSignPub);
TO_JSON(signAddrList);
TO_JSON(signThreshold);
D_END_R

PAUSE_D_REQ(getBonus_req)
PARSE_JSON(Addr)
D_END
DOUMP_D_REQ(getBonus_req)
TO_JSON(Addr)
D_END_R


PAUSE_D_REQ(getStakeUtxo_req)
PARSE_JSON(fromAddr)
D_END
DOUMP_D_REQ(getStakeUtxo_req)
TO_JSON(fromAddr)
D_END_R



DOUMP_D_ACK(getStakeUtxo_ack)

nlohmann::json jsonUtxos;
for (auto iter = utxos.begin(); iter != utxos.end(); iter++) {
	nlohmann::json jsonUtxo;
	jsonUtxo["utxo"] = iter->first;
	jsonUtxo["value"] = iter->second;
	jsonUtxos.push_back(jsonUtxo);
}
jsObject["utxos"] = jsonUtxos;
D_END_R

PAUSE_D_ACK(rpc_ack)
D_END

DOUMP_D_ACK(rpc_ack)
D_END_R


PAUSE_D_REQ(RSA_CODE)
PARSE_JSON(isEcode)
PARSE_JSON(strEncTxt)
PARSE_JSON(cipher_text)
PARSE_JSON(sign_message)
PARSE_JSON(strpub)
D_END

DOUMP_D_REQ(RSA_CODE)
TO_JSON(isEcode)
TO_JSON(strEncTxt)
TO_JSON(cipher_text)
TO_JSON(sign_message)
TO_JSON(strpub)
D_END_R


PAUSE_D_ACK(RSA_PUBSTR_ack)
PARSE_JSON(rsa_pubstr)
D_END

DOUMP_D_ACK(RSA_PUBSTR_ack)
TO_JSON(rsa_pubstr)
D_END_R


PAUSE_D_REQ(getDisInvestUtxo_req)
PARSE_JSON(fromAddr)
PARSE_JSON(toAddr)
D_END
DOUMP_D_REQ(getDisInvestUtxo_req)
TO_JSON(fromAddr)
TO_JSON(toAddr)
D_END_R

PAUSE_D_ACK(getDisInvestUtxo_ack)
PARSE_JSON(utxos)
D_END

DOUMP_D_ACK(getDisInvestUtxo_ack)
TO_JSON(utxos)
D_END_R