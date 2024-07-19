#include "dispatcher.h"

#include <utility>
#include <string>

#include "./global.h"
#include "./key_exchange.h"
#include "../utils/magic_singleton.h"

#include "../common/global.h"
#include "../common/task_pool.h"
#include "../include/logging.h"
#include "../proto/common.pb.h"
#include "../utils/compress.h"

int ProtobufDispatcher::Handle(const MsgData &data)
{
    CommonMsg commonMsg;
    int ret = commonMsg.ParseFromString(data.pack.data);
    if (!ret)
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

    std::string subSerializeMsg;
    if (commonMsg.compress())
    {
        Compress uncpr(std::move(commonMsg.data()), commonMsg.data().size() * 10);
        subSerializeMsg = uncpr._rawData;
    }
    else
    {
        subSerializeMsg = std::move(commonMsg.data());
    }
      std::string str_plaintext;
    if(type != "KeyExchangeRequest" && type != "KeyExchangeResponse")
    {
        Ciphertext ciphertext;
        if(!ciphertext.ParseFromString(subSerializeMsg))
        {
            ERRORLOG("ParseFromString Ciphertext fail!!!");
            return -6;
        }
        if ((ciphertext.aes_iv_12bytes().size() != CRYPTO_AES_IV_LEN)
                || (ciphertext.aes_tag_16bytes().size() != CRYPTO_AES_TAG_LEN))
            {
                ERRORLOG("the length of aes iv or tag does not match.");
                return -7;
            }

            auto key = MagicSingleton<KeyExchangeManager>::GetInstance()->getKey(data.fd);
            if(key == nullptr)
            {
                ERRORLOG("null key");
                return -8;
            }

            if (!verify_token(key->peer_key.ec_pub_key, ciphertext.token()))
            {
                ERRORLOG("token check failed.");
                return -9;
            }

            if (!decrypt_ciphertext(key->peer_key, ciphertext, str_plaintext))
            {
                ERRORLOG("aes decryption error.");
                return -10;
            }
    }
    else
    {
        str_plaintext = std::move(subSerializeMsg);
    }
    MessagePtr subMsg(proto->New());
    ret = subMsg->ParseFromString(str_plaintext);
    if (!ret)
    {
        ERRORLOG("bad msg for protobuf for {}", type.c_str());
        return -11;
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

    return -12;
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
