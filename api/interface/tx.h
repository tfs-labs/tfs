
#ifndef __TX_H__
#define __TX_H__
#include <string>
#include <vector>
#include <map>
#include "./base64.h"

#define PAUSE bool paseFromJson(std::string json);
#define DOUMP std::string paseToString();

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
	bool paseFromJson(std::string json);
	std::string paseToString();
};





UCTS_REQ(getsigvalue_req)
std::string addr;
std::string message;
UCTE

UCTS_ACK(getsigvalue_ack)
std::string signature;
std::string pub;
UCTE




UCTS_REQ(balance_req)
std::string addr;


UCTE


UCTS_ACK(balance_ack)
std::string addr;
std::string balance;
UCTE


UCTS_REQ(contract_info)
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

UCTS_REQ(getStake_req)
std::string fromAddr;
std::string stake_amount;
std::string PledgeType;
UCTE


UCTS_REQ(getUnstake_req)
std::string fromAddr;
std::string utxo_hash;
UCTE

UCTS_REQ(getInvest_req)
std::string fromAddr;
std::string toAddr;
std::string invest_amount;
std::string investType;
UCTE

UCTS_REQ(getDisInvest_req)
std::string fromAddr;
std::string toAddr;
std::string utxo_hash;
UCTE

UCTS_REQ(getDeclare_req)
std::string fromAddr;
std::string toAddr;
std::string amount;
std::string multiSignPub;
std::vector<std::string> signAddrList;
std::string signThreshold;
UCTE

UCTS_REQ(getBonus_req)
std::string Addr;
UCTE

UCTS_ACK(getStakeUtxo_req)
std::string fromAddr;
UCTE

UCTS_ACK(getStakeUtxo_ack)
std::map<std::string,uint64_t> utxos;
UCTE

UCTS_ACK(rpc_ack)
UCTE

UCTS_REQ(RSA_CODE)
std::string isEcode;
std::string strEncTxt;
std::string cipher_text;
std::string sign_message;
std::string strpub;
UCTE

UCTS_ACK(RSA_PUBSTR_ack)
std::string rsa_pubstr;
UCTE

UCTS_REQ(getDisInvestUtxo_req)
std::string fromAddr;
std::string toAddr;
UCTE


UCTS_ACK(getDisInvestUtxo_ack)
std::vector<std::string> utxos;
UCTE

#endif
