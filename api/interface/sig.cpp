#include "sig.h"
#include "tx.h"
#include "base64.h"
#include "proto/ca_protomsg.pb.h"
#include "net/httplib.h"
#include "utils/tmp_log.h"
#include "utils/base58.h"
#include "utils/account_manager.h"
bool JsonrpcGetSigvalue(const std::string& addr, const std::string& message, std::string & signature, std::string& pub)
{
    return true;
}

int AddMutilSignRpc(const std::string & addr, CTransaction &tx)
{	
	return 0;
}