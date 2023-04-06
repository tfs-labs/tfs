#ifndef _HANDLE_EVENT_H_
#define _HANDLE_EVENT_H_

#include <memory>
#include "net.pb.h"
#include "./msg_queue.h"
#include "ca_protomsg.pb.h"
#include "proto/interface.pb.h"
#include "../common/task_pool.h"

int handlePrintMsgReq(const std::shared_ptr<PrintMsgReq> &printMsgReq, const MsgData &from);
int handleRegisterNodeReq(const std::shared_ptr<RegisterNodeReq> &registerNode, const MsgData &from);
int handleRegisterNodeAck(const std::shared_ptr<RegisterNodeAck> &registerNodeAck, const MsgData &from);
int VerifyRegisterNode(const NodeInfo &nodeinfo, uint32_t &from_ip, uint32_t &from_port);
int handleBroadcastMsgReq(const std::shared_ptr<BroadcastMsgReq> &broadcaseMsgReq, const MsgData &from);

int handlePingReq(const std::shared_ptr<PingReq> &pingReq, const MsgData &from);
int handlePongReq(const std::shared_ptr<PongReq> &pongReq, const MsgData &from);
int handleSyncNodeReq(const std::shared_ptr<SyncNodeReq> &syncNodeReq, const MsgData &from);
int handleSyncNodeAck(const std::shared_ptr<SyncNodeAck> &syncNodeAck, const MsgData &from);
int handleEchoReq(const std::shared_ptr<EchoReq> &echoReq, const MsgData &from);
int handleEchoAck(const std::shared_ptr<EchoAck> &echoAck, const MsgData &from);
int handleNetTestReq(const std::shared_ptr<TestNetReq> &testReq, const MsgData &from);
int handleNetTestAck(const std::shared_ptr<TestNetAck> &testAck, const MsgData &from);

int handleNodeHeightChangedReq(const std::shared_ptr<NodeHeightChangedReq>& req, const MsgData& from);
int handleNodeBase58AddrChangedReq(const std::shared_ptr<NodeBase58AddrChangedReq>& req, const MsgData& from);

int handleBroadcastMsg( const std::shared_ptr<BuildBlockBroadcastMsg>& msg, const MsgData& msgdata);

int handleCheckTxReq(const std::shared_ptr<CheckTxReq>& req, const MsgData& from);
int handleCheckTxAck(const std::shared_ptr<CheckTxAck>& ack, const MsgData& from);



class netTest
{
    
public:
    netTest():netTestTime(-1),signal(true),flag(false) {}

    void setTime(double t)
    {
        std::unique_lock<std::mutex> lock(mutex_time);
        flag = true;
        netTestTime = t;
    }

    void isValue()
    {
        int i = 0;
        for(; i < 20; i++)
        {
            if(netTestTime == -1)
            {
                sleep(1);
                continue;
            }
            break;
        }

        std::unique_lock<std::mutex> lock(mutex_time);
        signal = true;
    }

    double getTime()
    {
        std::unique_lock<std::mutex> lock(mutex_time);
        double t = netTestTime;
        netTestTime = -1;
        flag = false;
        return t;
    }

    bool getSignal()
    {
        return signal;
    }
    bool getflag()
    {
        return flag;
    }

    void netTestRegister();
private:
    double netTestTime;
    bool signal;
    bool flag;
    std::mutex mutex_time;
};

#endif
