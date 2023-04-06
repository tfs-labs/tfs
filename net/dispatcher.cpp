#include "dispatcher.h"
#include <string>
#include "../include/logging.h"
#include "net.pb.h"
#include "common.pb.h"
#include "handle_event.h"
#include "./peer_node.h"
#include "utils/compress.h"
#include <utility>
#include "global.h"
#include "common/global.h"
#include "logging.h"
#include "common/task_pool.h"

int ProtobufDispatcher::handle(const MsgData &data)
{
    CommonMsg common_msg;
    int r = common_msg.ParseFromString(data.pack.data);
    if (!r)
    {
        ERRORLOG("parse CommonMsg error");
        return -1;
    }

    std::string type = common_msg.type();
    {
        std::lock_guard<std::mutex> lock(global::g_mutex_req_cnt_map);
        global::reqCntMap[type].first += 1;
        global::reqCntMap[type].second += common_msg.data().size();
    }
    
    if (common_msg.version() != global::kNetVersion)
    {
        ERRORLOG("common_msg.version() {}", common_msg.version());
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

    string sub_serialize_msg;
    if (common_msg.compress())
    {
        Compress uncpr(std::move(common_msg.data()), common_msg.data().size() * 10);
        sub_serialize_msg = uncpr.m_raw_data;
    }
    else
    {
        sub_serialize_msg = std::move(common_msg.data());
    }

    MessagePtr sub_msg(proto->New());
    r = sub_msg->ParseFromString(sub_serialize_msg);
    if (!r)
    {
        ERRORLOG("bad msg for protobuf for {}", type.c_str());
        return -6;
    }

    auto task_pool = MagicSingleton<taskPool>::GetInstance();
    std::string name = sub_msg->GetDescriptor()->name();

    auto block_map = block_protocbs_.find(name);
    if (block_map != block_protocbs_.end())
    {
        task_pool->commit_block_task(block_map->second, sub_msg, data);
        return 0;
    }

    auto saveBlock_map = saveBlock_protocbs_.find(name);
    if (saveBlock_map != saveBlock_protocbs_.end())
    {
        task_pool->commit_saveBlock_task(saveBlock_map->second, sub_msg, data);
    }

    auto broadcast_map= broadcast_protocbs_.find(name);
    if (broadcast_map != broadcast_protocbs_.end())
    {
        task_pool->commit_broadcast_task(broadcast_map->second, sub_msg, data);
        return 0;
    }

    auto ca_map = ca_protocbs_.find(name);
    if (ca_map != ca_protocbs_.end())
    {
        task_pool->commit_ca_task(ca_map->second, sub_msg, data);
        return 0;
    }

    auto net_map = net_protocbs_.find(name);
    if (net_map != net_protocbs_.end())
    {
        task_pool->commit_net_task(net_map->second, sub_msg, data);
        return 0;
    }
    auto tx_map = tx_protocbs_.find(name);
    if (tx_map != tx_protocbs_.end())
    {
        task_pool->commit_tx_task(tx_map->second, sub_msg, data);
        return 0;
    }
    auto syncBlock_map = syncBlock_protocbs_.find(name);
    if (syncBlock_map != syncBlock_protocbs_.end())
    {
        task_pool->commit_syncBlock_task(syncBlock_map->second, sub_msg, data);
        return 0;
    }

    return -7;
}

void ProtobufDispatcher::registerAll()
{
    MagicSingleton<taskPool>::GetInstance()->taskPool_init();
    
    net_registerCallback<RegisterNodeReq>(handleRegisterNodeReq);
    net_registerCallback<RegisterNodeAck>(handleRegisterNodeAck);
    net_registerCallback<SyncNodeReq>(handleSyncNodeReq);
    net_registerCallback<SyncNodeAck>(handleSyncNodeAck);
    
    net_registerCallback<BroadcastMsgReq>(handleBroadcastMsgReq);

    net_registerCallback<CheckTxReq>(handleCheckTxReq);
    net_registerCallback<CheckTxAck>(handleCheckTxAck);
    
    net_registerCallback<PrintMsgReq>(handlePrintMsgReq);
    net_registerCallback<PingReq>(handlePingReq);
    net_registerCallback<PongReq>(handlePongReq);
    net_registerCallback<EchoReq>(handleEchoReq);
    net_registerCallback<EchoAck>(handleEchoAck);
    net_registerCallback<TestNetAck>(handleNetTestAck);
    net_registerCallback<TestNetReq>(handleNetTestReq);
    net_registerCallback<NodeHeightChangedReq>(handleNodeHeightChangedReq);
    net_registerCallback<NodeBase58AddrChangedReq>(handleNodeBase58AddrChangedReq);
    broadcast_registerCallback<BuildBlockBroadcastMsg>(handleBroadcastMsg);
}

void ProtobufDispatcher::task_info(std::ostringstream& oss)
{
    auto task_pool = MagicSingleton<taskPool>::GetInstance();
    oss << "ca_active_task:" << task_pool->ca_active() << std::endl;
    oss << "ca_pending_task:" << task_pool->ca_pending() << std::endl;
    oss << "==================================" << std::endl;
    oss << "net_active_task:" << task_pool->net_active() << std::endl;
    oss << "net_pending_task:" << task_pool->net_pending() << std::endl;
    oss << "==================================" << std::endl;
    oss << "broadcast_active_task:" << task_pool->broadcast_active() << std::endl;
    oss << "broadcast_pending_task:" << task_pool->broadcast_pending() << std::endl;
    oss << "==================================" << std::endl;
    oss << "tx_active_task:" << task_pool->tx_active() << std::endl;
    oss << "tx_pending_task:" << task_pool->tx_pending() << std::endl;
    oss << "==================================" << std::endl;
    oss << "syncBlock_active_task:" << task_pool->syncBlock_active() << std::endl;
    oss << "syncBlock_pending_task:" << task_pool->syncBlock_pending() << std::endl;
    oss << "==================================" << std::endl;
    oss << "saveBlock_active_task:" << task_pool->saveBlock_active() << std::endl;
    oss << "saveBlock_pending_task:" << task_pool->saveBlock_pending() << std::endl;
    oss << "==================================" << std::endl;
    oss << "block_active_task:" << task_pool->block_active() << std::endl;
    oss << "block_pending_task:" << task_pool->block_pending() << std::endl;
    oss << "==================================" << std::endl;
    oss << "work_active_task:" << task_pool->work_active() << std::endl;
    oss << "work_pending_task:" << task_pool->work_pending() << std::endl;
    oss << "==================================" << std::endl;

}
