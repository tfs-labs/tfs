#include "sig.h"
#include "tx.h"
#include "base64.h"
#include "proto/ca_protomsg.pb.h"
#include "net/httplib.h"
#include "utils/tmplog.h"
#include "utils/base58.h"
#include "utils/AccountManager.h"
bool jsonrpc_get_sigvalue(const std::string& addr, const std::string& message, std::string & signature, std::string& pub)
{
    httplib::Headers headers = {
          { "content-type", "application/json" }
    };
    httplib::Client cli("192.168.1.141", 3309);

    getsigvalue_req req;
    req.addr = addr;
    req.message = message;

    std::string json_body = req.paseToString();

    std::shared_ptr<httplib::Response> res = cli.Post("/getsigvalue_req", headers, json_body, "application/json");

    getsigvalue_ack ack;
    ack.paseFromJson(res->body);

    if (ack.ErrorCode != "0") {
        errorL("error:" << ack.ErrorMessage);
        return false;
    }
    Base64 base_;
    signature = base_.Decode((const  char *)ack.signature.c_str(),ack.signature.size());
    pub =  base_.Decode((const  char *)ack.pub.c_str(),ack.pub.size());
    return true;
}

int AddMutilSign_rpc(const std::string & addr, CTransaction &tx)
{
	if (!CheckBase58Addr(addr))
	{
		return -1;
	}

	CTxUtxo * txUtxo = tx.mutable_utxo();
	CTxUtxo copyTxUtxo = *txUtxo;
	copyTxUtxo.clear_multisign();

	std::string serTxUtxo = getsha256hash(copyTxUtxo.SerializeAsString());
	std::string signature;
	std::string pub;
	if(jsonrpc_get_sigvalue(addr, serTxUtxo, signature, pub) == false)
	{
		return -2;
	}

	CSign * multiSign = txUtxo->add_multisign();
	multiSign->set_sign(signature);
	multiSign->set_pub(pub);

	return 0;
}