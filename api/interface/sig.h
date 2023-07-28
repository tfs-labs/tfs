#ifndef __SIG_RPC_
#define __SIG_RPC_
#include "proto/transaction.pb.h"
#include <string>
bool jsonrpc_get_sigvalue(const std::string& addr, const std::string& message, std::string & signature, std::string& pub);

int AddMutilSign_rpc(const std::string & addr, CTransaction &tx);
#endif