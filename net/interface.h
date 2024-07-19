#ifndef _NET_INTERFACE_H_
#define _NET_INTERFACE_H_


#include <iostream>
#include <map>
#include <vector>
#include <unordered_map>
#include "../net/api.h"
#include "../net/msg_queue.h"
#include "../net/dispatcher.h"

static bool NetInit()
{
    // register
    MagicSingleton<TaskPool>::GetInstance()->TaskPoolInit();
    
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<RegisterNodeReq>(HandleRegisterNodeReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<RegisterNodeAck>(HandleRegisterNodeAck);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<SyncNodeReq>(HandleSyncNodeReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<SyncNodeAck>(HandleSyncNodeAck);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<CheckTxReq>(HandleCheckTxReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<CheckTxAck>(HandleCheckTxAck);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<GetUtxoHashReq>(HandleGetTxUtxoHashReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<GetUtxoHashAck>(HandleGetTxUtxoHashAck);
    
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<PrintMsgReq>(HandlePrintMsgReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<PingReq>(HandlePingReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<PongReq>(HandlePongReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<EchoReq>(HandleEchoReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<EchoAck>(HandleEchoAck);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<TestNetAck>(HandleNetTestAck);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<TestNetReq>(HandleNetTestReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<NodeHeightChangedReq>(HandleNodeHeightChangedReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<NodeAddrChangedReq>(HandleNodeAddrChangedReq);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<KeyExchangeRequest>(HandleKeyExchangeReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<KeyExchangeResponse>(HandleKeyExchangeAck);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->BroadcastRegisterCallback<BuildBlockBroadcastMsg>(HandleBroadcastMsg);

    net_com::InitializeNetwork();
    return true;
}

/**
 * @brief    Single point to send information, T type is the type of protobuf protocol   
 * 
 */
template <typename T>
bool NetSendMessage(const std::string & id, 
                        const T & msg, 
                        const net_com::Compress isCompress, 
                        const net_com::Encrypt isEncrypt, 
                        const net_com::Priority priority)
{
    return net_com::SendMessage(id, msg, isCompress, isEncrypt, priority);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const std::string & id, 
                        const T & msg)
{
    return net_com::SendMessage(id, msg, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const std::string & id, 
                        const T & msg, 
                        const net_com::Compress isCompress)
{
    return net_com::SendMessage(id, msg, isCompress, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const std::string & id, 
                        const T & msg, 
                        const net_com::Encrypt isEncrypt)
{
    return net_com::SendMessage(id, msg, net_com::Compress::kCompress_False, isEncrypt, net_com::Priority::kPriority_Low_0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const std::string & id, 
                        const T & msg, 
                        const net_com::Priority priority)
{
    return net_com::SendMessage(id, msg, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, priority);
}



/**
 * @brief    To send information with a receipt address, type T is the type of protobuf protocol 
 * 
 */
template <typename T>
bool NetSendMessage(const MsgData & from, 
                        const T & msg, 
                        const net_com::Compress isCompress, 
                        const net_com::Encrypt isEncrypt, 
                        const net_com::Priority priority)
{
    return net_com::SendMessage(from, msg, isCompress, isEncrypt, priority);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const MsgData & from, 
                        const T & msg)
{
    return net_com::SendMessage(from, msg, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const MsgData & from, 
                        const T & msg, 
                        const net_com::Compress isCompress)
{
    return net_com::SendMessage(from, msg, isCompress, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const MsgData & from, 
                        const T & msg, 
                        const net_com::Encrypt isEncrypt)
{
    return net_com::SendMessage(from, msg, net_com::Compress::kCompress_False, isEncrypt, net_com::Priority::kPriority_Low_0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const MsgData & from, 
                        const T & msg,
                        const net_com::Priority priority)
{
    return net_com::SendMessage(from, msg, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, priority);
}

/**
 * @brief      Broadcast information
 * 
 */
template <typename T>
bool NetBroadcastMessage(const T& msg, 
                            const net_com::Compress isCompress, 
                            const net_com::Encrypt isEncrypt, 
                            const net_com::Priority priority)
{
    return net_com::BroadCastMessage(msg, isCompress, isEncrypt, priority);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetBroadcastMessage(const T& msg)
{
    return net_com::BroadCastMessage(msg, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetBroadcastMessage(const T& msg, 
                            const net_com::Compress isCompress)
{
    return net_com::BroadCastMessage(msg, isCompress, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetBroadcastMessage(const T& msg, 
                            const net_com::Encrypt isEncrypt)
{
    return net_com::BroadCastMessage(msg, net_com::Compress::kCompress_False, isEncrypt, net_com::Priority::kPriority_Low_0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetBroadcastMessage(const T& msg, 
                            const net_com::Priority priority)
{
    return net_com::BroadCastMessage(msg, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, priority);
}


/**
 * @brief       Send node which was changed
 * 
 */

static void NetSendNodeHeightChanged()
{
	net_com::SendNodeHeightChanged();
}




/**
 * @brief       
 * 
 */
template <typename T>
void NetUnregisterCallback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetUnregisterCallback<T>();
}

/**
 * @brief       
 * 
 */
template <typename T>
void CaUnregisterCallback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaUnregisterCallback<T>();
}

/**
 * @brief       
 * 
 */
template <typename T>
void BroadcastUnregisterCallback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->BroadcastUnregisterCallback<T>();
}

/**
 * @brief       
 * 
 */
template <typename T>
void TxUnregisterCallback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->TxUnregisterCallback<T>();
}

/**
 * @brief       
 * 
 */
template <typename T>
void SyncBlockUnregisterCallback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockUnregisterCallback<T>();
}

/**
 * @brief       
 * 
 */
template <typename T>
void SaveBlockUnregisterCallback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SaveBlockUnregisterCallback<T>();
}

/**
 * @brief       
 * 
 */
template <typename T>
void BlockUnregisterCallback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->BlockUnregisterCallback<T>();
}

#endif

