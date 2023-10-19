#include "dispatcher.h"

#include <utility>
#include <string>

#include "./global.h"
#include "./handle_event.h"
#include "./peer_node.h"

#include "../ca/interface.h"
#include "../ca/transaction.h"
#include "../common/global.h"
#include "../common/task_pool.h"
#include "../include/logging.h"
#include "../proto/common.pb.h"
#include "../proto/net.pb.h"
#include "../utils/compress.h"
int ProtobufDispatcher::Handle(const MsgData &data)
{
    CommonMsg commonMsg;
    int r = commonMsg.ParseFromString(data.pack.data);
    if (!r)
    {
        ERRORLOG("parse CommonMsg error");
        return -1;
    }

    std::string type = commonMsg.type();
    {
        std::lock_guard<std::mutex> lock(global::g_mutexReqCntMap);
        global::g_reqCntMap[type].first += 1;
        global::g_reqCntMap[type].second += commonMsg.data().size();
    }
    
    if (commonMsg.version() != global::kNetVersion)
    {
        ERRORLOG("commonMsg.version() {}", commonMsg.version());
        return -2;
    }

    if (type.size() == 0)
    {
        ERRORLOG("handle type is empty");
        return -3;
    }

    const Descriptor *des = google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(type);
    if (!des)
    {
        ERRORLOG("cannot create Descriptor for {}", type.c_str());
        return -4;
    }

    const Message *proto = google::protobuf::MessageFactory::generated_factory()->GetPrototype(des);
    if (!proto)
    {
        ERRORLOG("cannot create Message for {}", type.c_str());
        return -5;
    }

    string subSerializeMsg;
    if (commonMsg.compress())
    {
        Compress uncpr(std::move(commonMsg.data()), commonMsg.data().size() * 10);
        subSerializeMsg = uncpr._rawData;
    }
    else
    {
        subSerializeMsg = std::move(commonMsg.data());
    }

    MessagePtr subMsg(proto->New());
    r = subMsg->ParseFromString(subSerializeMsg);
    if (!r)
    {
        ERRORLOG("bad msg for protobuf for {}", type.c_str());
        return -6;
    }

    auto taskPool = MagicSingleton<TaskPool>::GetInstance();
    std::string name = subMsg->GetDescriptor()->name();

    auto blockMap = _blockProtocbs.find(name);
    if (blockMap != _blockProtocbs.end())
    {
        taskPool->CommitBlockTask(blockMap->second, subMsg, data);
        return 0;
    }

    auto saveBlockmap = _saveBlockProtocbs.find(name);
    if (saveBlockmap != _saveBlockProtocbs.end())
    {
        taskPool->CommitSaveBlockTask(saveBlockmap->second, subMsg, data);
    }

    auto broadcastMap= _broadcastProtocbs.find(name);
    if (broadcastMap != _broadcastProtocbs.end())
    {
        taskPool->CommitBroadcastTask(broadcastMap->second, subMsg, data);
        return 0;
    }

    auto caMap = _caProtocbs.find(name);
    if (caMap != _caProtocbs.end())
    {
        taskPool->CommitCaTask(caMap->second, subMsg, data);
        return 0;
    }

    auto netMap = _netProtocbs.find(name);
    if (netMap != _netProtocbs.end())
    {
        taskPool->CommitNetTask(netMap->second, subMsg, data);
        return 0;
    }
    auto txMap = _txProtocbs.find(name);
    if (txMap != _txProtocbs.end())
    {
        taskPool->CommitTxTask(txMap->second, subMsg, data);
        return 0;
    }
    auto syncBlockMap = _syncBlockProtocbs.find(name);
    if (syncBlockMap != _syncBlockProtocbs.end())
    {
        taskPool->CommitSyncBlockTask(syncBlockMap->second, subMsg, data);
        return 0;
    }

    return -7;
}

void ProtobufDispatcher::RetisterAll()
{
    MagicSingleton<TaskPool>::GetInstance()->TaskPoolInit();
    
    NetRegisterCallback<RegisterNodeReq>(HandleRegisterNodeReq);
    NetRegisterCallback<RegisterNodeAck>(HandleRegisterNodeAck);
    NetRegisterCallback<SyncNodeReq>(HandleSyncNodeReq);
    NetRegisterCallback<SyncNodeAck>(HandleSyncNodeAck);
    
    NetRegisterCallback<BroadcastMsgReq>(HandleBroadcastMsgReq);

    NetRegisterCallback<CheckTxReq>(HandleCheckTxReq);
    NetRegisterCallback<CheckTxAck>(HandleCheckTxAck);
    
    NetRegisterCallback<PrintMsgReq>(HandlePrintMsgReq);
    NetRegisterCallback<PingReq>(HandlePingReq);
    NetRegisterCallback<PongReq>(HandlePongReq);
    NetRegisterCallback<EchoReq>(HandleEchoReq);
    NetRegisterCallback<EchoAck>(HandleEchoAck);
    NetRegisterCallback<TestNetAck>(HandleNetTestAck);
    NetRegisterCallback<TestNetReq>(HandleNetTestReq);
    NetRegisterCallback<NodeHeightChangedReq>(HandleNodeHeightChangedReq);
    NetRegisterCallback<NodeBase58AddrChangedReq>(HandleNodeBase58AddrChangedReq);
    BroadcastRegisterCallback<BuildBlockBroadcastMsg>(HandleBroadcastMsg);
}

void ProtobufDispatcher::TaskInfo(std::ostringstream& oss)
{
    auto taskPool = MagicSingleton<TaskPool>::GetInstance();
    oss << "ca_active_task:" << taskPool->CaActive() << std::endl;
    oss << "ca_pending_task:" << taskPool->CaPending() << std::endl;
    oss << "==================================" << std::endl;
    oss << "net_active_task:" << taskPool->NetActive() << std::endl;
    oss << "net_pending_task:" << taskPool->NetPending() << std::endl;
    oss << "==================================" << std::endl;
    oss << "broadcast_active_task:" << taskPool->BroadcastActive() << std::endl;
    oss << "broadcast_pending_task:" << taskPool->BroadcastPending() << std::endl;
    oss << "==================================" << std::endl;
    oss << "tx_active_task:" << taskPool->TxActive() << std::endl;
    oss << "tx_pending_task:" << taskPool->TxPending() << std::endl;
    oss << "==================================" << std::endl;
    oss << "syncBlock_active_task:" << taskPool->SyncBlockActive() << std::endl;
    oss << "syncBlock_pending_task:" << taskPool->SyncBlockPending() << std::endl;
    oss << "==================================" << std::endl;
    oss << "saveBlock_active_task:" << taskPool->SaveBlockActive() << std::endl;
    oss << "saveBlock_pending_task:" << taskPool->SaveBlockPending() << std::endl;
    oss << "==================================" << std::endl;
    oss << "block_active_task:" << taskPool->BlockActive() << std::endl;
    oss << "block_pending_task:" << taskPool->BlockPending() << std::endl;
    oss << "==================================" << std::endl;
    oss << "work_active_task:" << taskPool->WorkActive() << std::endl;
    oss << "work_pending_task:" << taskPool->WorkPending() << std::endl;
    oss << "==================================" << std::endl;

}
