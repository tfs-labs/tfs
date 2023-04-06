#ifndef _TRAN_STROAGE_
#define _TRAN_STROAGE_

#include "ca_txhelper.h"
#include "ca_global.h"
#include "proto/transaction.pb.h"
#include "proto/ca_protomsg.pb.h"
#include "net/msg_queue.h"
#include "utils/time_util.h"
#include "ca/ca_transaction_cache.h"

#include <net/if.h>
#include <unistd.h>
#include <shared_mutex>
#include <map>

class TranStroage
{
public:
    TranStroage(){ Start_Timer(); };
    ~TranStroage() = default;
    TranStroage(TranStroage &&) = delete;
    TranStroage(const TranStroage &) = delete;
    TranStroage &operator=(TranStroage &&) = delete;
    TranStroage &operator=(const TranStroage &) = delete;

public:
	int Add(uint64_t height,const TxMsgReq& msg);
private:
	void Start_Timer();
    bool is_empty();
	void Check();
private:
    mutable std::shared_mutex txPending_mutex;
    std::map<uint64_t, std::vector<TxMsgReq>> txPending;
	CTimer _timer;
};

#endif