
#ifndef __TX_H__
#define __TX_H__
#include <string>
#include <vector>
#include <map>
#include "./base64.h"
#include "utils/json.hpp"

#define PAUSE bool paseFromJson(const std::string& json);
#define DOUMP std::string paseToString();
#define TOJSONOBJ nlohmann::json paseToJsonObj(const std::string & json_str);
#define UCTS_ACK(name)\
	struct name{\
	PAUSE\
	DOUMP\
	std::string type;\
	std::string ErrorCode;\
	std::string ErrorMessage;

#define UCTS_REQ(name)\
	struct name{\
	PAUSE\
	DOUMP\
	std::string type;

#define UCTE };

//the_top
struct the_top {
	std::string type;
	std::string top;
	std::string ErrorCode;
	std::string ErrorMessage;
	bool paseFromJson(const std::string& json);
	std::string paseToString();
};









UCTS_REQ(balance_req)
std::string addr;
UCTE


UCTS_ACK(balance_ack)
std::string addr;
std::string balance;
UCTE


UCTS_REQ(contract_info)
TOJSONOBJ
std::string name;
std::string language;
std::string languageVersion;
std::string standard;
std::string logo;
std::string source;
std::string ABI;
std::string userDoc;
std::string developerDoc;
std::string compilerVersion;
std::string compilerOptions;
std::string srcMap;
std::string srcMapRuntime;
std::string metadata;
std::string other;
UCTE



UCTS_REQ(deploy_contract_req)
std::string addr;
std::string nContractType;
std::string info;
std::string contract;
std::string data;
std::string pubstr;
UCTE





UCTS_REQ(deploy_utxo_req)
std::string addr;
UCTE


UCTS_ACK(deploy_utxo_ack)
std::vector<std::string> utxos;
UCTE



UCTS_REQ(call_contract_req)
std::string addr;
std::string deployer;
std::string deployutxo;
std::string args;
std::string pubstr;
std::string tip;
std::string money;
UCTE


UCTS_ACK(call_contract_ack)

};



UCTS_REQ(deployers_req)
UCTE

UCTS_ACK(deployers_ack)
std::vector<std::string> deployers;
UCTE



UCTS_REQ(tx_req)
std::vector<std::string> fromAddr;
std::map<std::string, std::string> toAddr;
UCTE




UCTS_ACK(tx_ack)
std::string txJson;
std::string height;
std::string vrfJson;
std::string txType;
std::string time;
std::string gas;
UCTE



UCTS_REQ(get_stake_req)
std::string fromAddr;
std::string stake_amount;
std::string PledgeType;
UCTE


UCTS_REQ(get_unstake_req)
std::string fromAddr;
std::string utxo_hash;
UCTE

UCTS_REQ(get_invest_req)
std::string fromAddr;
std::string toAddr;
std::string invest_amount;
std::string investType;
UCTE

UCTS_REQ(get_disinvest_req)
std::string fromAddr;
std::string toAddr;
std::string utxo_hash;
UCTE

UCTS_REQ(get_declare_req)
std::string fromAddr;
std::string toAddr;
std::string amount;
std::string multiSignPub;
std::vector<std::string> signAddrList;
std::string signThreshold;
UCTE

UCTS_REQ(get_bonus_req)
std::string Addr;
UCTE

UCTS_ACK(get_stakeutxo_req)
std::string fromAddr;
UCTE

UCTS_ACK(get_stakeutxo_ack)
std::map<std::string,uint64_t> utxos;
UCTE

UCTS_ACK(rpc_ack)
std::string txhash;
UCTE


UCTS_REQ(rsa_code)
std::string isEcode;
std::string strEncTxt;
std::string cipher_text;
std::string sign_message;
std::string strpub;
UCTE

UCTS_ACK(rsa_pubstr_ack)
std::string rsa_pubstr;
UCTE

UCTS_REQ(get_disinvestutxo_req)
std::string fromAddr;
std::string toAddr;
UCTE


UCTS_ACK(get_disinvestutxo_ack)
std::vector<std::string> utxos;
UCTE


UCTS_REQ(get_isonchain_req)
std::string txhash;
UCTE

UCTS_ACK(get_isonchain_ack)
std::string txhash;
std::string pro;
UCTE

UCTS_REQ(get_restinverst_req)
std::string addr;
UCTE

UCTS_ACK(get_restinverst_ack)
std::string addr;
std::string amount;
std::string message;
UCTE



UCTS_REQ(get_tx_info_req)
std::string txhash;
UCTE

UCTS_ACK(get_tx_info_ack)
std::string tx;
uint64_t blockheight;
std::string blockhash;
UCTE

UCTS_REQ(stake_node_list)

std::string addr; 
std::string name; 
std::string ip; 
std::string identity; 
std::string logo; 
std::string height;
std::string version; 
UCTE

UCTS_ACK(get_all_stake_node_list_ack)
std::string version;
std::string code;
std::string message ;
std::vector<stake_node_list> list;
UCTE

UCTS_REQ(evm_to_base58_req)
std::string addr;
UCTE

UCTS_REQ(pubstr_to_evm_req)
std::string pubstr;
UCTE 

#endif
