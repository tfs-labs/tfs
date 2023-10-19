#ifndef _NET_INTERFACE_H_
#define _NET_INTERFACE_H_


#include <iostream>
#include <map>
#include <vector>
#include <unordered_map>
#include "../net/msg_queue.h"
#include "../net/api.h"
#include "../net/dispatcher.h"

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
 * @brief       Obtaining the ID of your own node returns 0 successfully
 * 
 * @return      std::string 
 */
std::string NetGetSelfNodeId();


/**
 * @brief       Get node information
 * 
 * @return      Node 
 */
Node NetGetSelfNode();

/**
 * @brief       Returns all node IDs
 * 
 * @return      std::vector<std::string> 
 */
std::vector<std::string>  NetGetNodeIds();

/**
 * @brief       Returns the public network node
 * 
 * @return      std::vector<Node> 
 */
std::vector<Node> NetGetPublicNode();

/**
 * @brief      Returns all public network nodes, regardless of whether they are connected or not 
 * 
 * @return      std::vector<Node> 
 */
std::vector<Node> NetGetAllPublicNode();

/**
 * @brief       Returns all node IDs and base58Address
 * 
 * @return      std::map<std::string, string> 
 */
std::map<std::string, string> NetGetNodeIdsAndBase58Address();

/**
 * @brief       Find the ID by IP
 * 
 * @param       ip 
 * @return      std::string 
 */
std::string NetGetIdByIp(std::string ip);

/**
 * @brief       Send node which was changed
 * 
 */
void NetSendNodeHeightChanged();

/**
 * @brief       Returns the percentage of nodes on the connection
 * 
 * @return      double 
 */
double NetGetConnectedPercent();

/**
 * @brief       
 * 
 */
template <typename T>
void NetRegisterCallback(std::function<int( const std::shared_ptr<T>& msg, const MsgData& from)> cb)
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<T>(cb);
}

/**
 * @brief       
 * 
 */
template <typename T>
void CaRegisterCallback(std::function<int( const std::shared_ptr<T>& msg, const MsgData& from)> cb)
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<T>(cb);
}

/**
 * @brief       
 * 
 */
template <typename T>
void TxRegisterCallback(std::function<int( const std::shared_ptr<T>& msg, const MsgData& from)> cb)
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->TxRegisterCallback<T>(cb);
}

/**
 * @brief       
 * 
 */
template <typename T>
void SyncBlockRegisterCallback(std::function<int( const std::shared_ptr<T>& msg, const MsgData& from)> cb)
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<T>(cb);
}

/**
 * @brief       
 * 
 */
template <typename T>
void SaveBlockRegisterCallback(std::function<int( const std::shared_ptr<T>& msg, const MsgData& from)> cb)
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SaveBlockRegisterCallback<T>(cb);
}

/**
 * @brief       
 * 
 */
template <typename T>
void BlockRegisterCallback(std::function<int( const std::shared_ptr<T>& msg, const MsgData& from)> cb)
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->BlockRegisterCallback<T>(cb);
}

/**
 * @brief       
 * 
 */
template <typename T>
void BroadcastRegisterCallback(std::function<int( const std::shared_ptr<T>& msg, const MsgData& from)> cb)
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->BroadcastRegisterCallback<T>(cb);
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

