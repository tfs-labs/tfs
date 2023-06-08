#ifndef _NET_INTERFACE_H_
#define _NET_INTERFACE_H_


#include <iostream>
#include <map>
#include <vector>
#include <unordered_map>
#include "../net/msg_queue.h"
#include "../net/net_api.h"
#include "../net/dispatcher.h"


//Single point to send information, T type is the type of protobuf protocol
template <typename T>
bool net_send_message(const std::string & id, 
                        const T & msg, 
                        const net_com::Compress isCompress, 
                        const net_com::Encrypt isEncrypt, 
                        const net_com::Priority priority)
{
    return net_com::send_message(id, msg, isCompress, isEncrypt, priority);
}

template <typename T>
bool net_send_message(const std::string & id, 
                        const T & msg)
{
    return net_com::send_message(id, msg, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
}

template <typename T>
bool net_send_message(const std::string & id, 
                        const T & msg, 
                        const net_com::Compress isCompress)
{
    return net_com::send_message(id, msg, isCompress, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
}

template <typename T>
bool net_send_message(const std::string & id, 
                        const T & msg, 
                        const net_com::Encrypt isEncrypt)
{
    return net_com::send_message(id, msg, net_com::Compress::kCompress_False, isEncrypt, net_com::Priority::kPriority_Low_0);
}

template <typename T>
bool net_send_message(const std::string & id, 
                        const T & msg, 
                        const net_com::Priority priority)
{
    return net_com::send_message(id, msg, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, priority);
}


//To send information with a receipt address, type T is the type of protobuf protocol
template <typename T>
bool net_send_message(const MsgData & from, 
                        const T & msg, 
                        const net_com::Compress isCompress, 
                        const net_com::Encrypt isEncrypt, 
                        const net_com::Priority priority)
{
    return net_com::send_message(from, msg, isCompress, isEncrypt, priority);
}

template <typename T>
bool net_send_message(const MsgData & from, 
                        const T & msg)
{
    return net_com::send_message(from, msg, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
}

template <typename T>
bool net_send_message(const MsgData & from, 
                        const T & msg, 
                        const net_com::Compress isCompress)
{
    return net_com::send_message(from, msg, isCompress, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
}

template <typename T>
bool net_send_message(const MsgData & from, 
                        const T & msg, 
                        const net_com::Encrypt isEncrypt)
{
    return net_com::send_message(from, msg, net_com::Compress::kCompress_False, isEncrypt, net_com::Priority::kPriority_Low_0);
}
template <typename T>
bool net_send_message(const MsgData & from, 
                        const T & msg,
                        const net_com::Priority priority)
{
    return net_com::send_message(from, msg, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, priority);
}


//Broadcast information
template <typename T>
bool net_broadcast_message(const T& msg, 
                            const net_com::Compress isCompress, 
                            const net_com::Encrypt isEncrypt, 
                            const net_com::Priority priority)
{
    return net_com::broadcast_message(msg, isCompress, isEncrypt, priority);
}

template <typename T>
bool net_broadcast_message(const T& msg)
{
    return net_com::broadcast_message(msg, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
}

template <typename T>
bool net_broadcast_message(const T& msg, 
                            const net_com::Compress isCompress)
{
    return net_com::broadcast_message(msg, isCompress, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
}

template <typename T>
bool net_broadcast_message(const T& msg, 
                            const net_com::Encrypt isEncrypt)
{
    return net_com::broadcast_message(msg, net_com::Compress::kCompress_False, isEncrypt, net_com::Priority::kPriority_Low_0);
}

template <typename T>
bool net_broadcast_message(const T& msg, 
                            const net_com::Priority priority)
{
    return net_com::broadcast_message(msg, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, priority);
}



//Obtaining the ID of your own node returns 0 successfully
std::string net_get_self_node_id();

//Get node information
Node net_get_self_node();

//Returns all node IDs
std::vector<std::string>  net_get_node_ids();

//Returns the public network node
std::vector<Node> net_get_public_node();

//Returns all public network nodes, regardless of whether they are connected or not
std::vector<Node> net_get_all_public_node();

//Returns all node IDs and base58address
std::map<std::string, string> net_get_node_ids_and_base58address();

//Find the ID by IP
std::string net_get_ID_by_ip(std::string ip);

// Send node which was changed, 20211129  Liu
void net_send_node_height_changed();

//Returns the percentage of nodes on the connection
double net_get_connected_percent();


template <typename T>
void net_register_callback(std::function<int( const std::shared_ptr<T>& msg, const MsgData& from)> cb)
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->net_registerCallback<T>(cb);
}
template <typename T>
void ca_register_callback(std::function<int( const std::shared_ptr<T>& msg, const MsgData& from)> cb)
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->ca_registerCallback<T>(cb);
}
template <typename T>
void tx_register_callback(std::function<int( const std::shared_ptr<T>& msg, const MsgData& from)> cb)
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->tx_registerCallback<T>(cb);
}
template <typename T>
void syncBlock_register_callback(std::function<int( const std::shared_ptr<T>& msg, const MsgData& from)> cb)
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->syncBlock_registerCallback<T>(cb);
}
template <typename T>
void saveBlock_register_callback(std::function<int( const std::shared_ptr<T>& msg, const MsgData& from)> cb)
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->saveBlock_registerCallback<T>(cb);
}
template <typename T>
void block_register_callback(std::function<int( const std::shared_ptr<T>& msg, const MsgData& from)> cb)
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->block_registerCallback<T>(cb);
}
template <typename T>
void broadcast_register_callback(std::function<int( const std::shared_ptr<T>& msg, const MsgData& from)> cb)
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->broadcast_registerCallback<T>(cb);
}



template <typename T>
void net_unregister_callback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->net_unRegisterCallback<T>();
}
template <typename T>
void ca_unregister_callback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->ca_unRegisterCallback<T>();
}
template <typename T>
void broadcast_unregister_callback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->broadcast_unRegisterCallback<T>();
}


template <typename T>
void tx_unregister_callback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->tx_unRegisterCallback<T>();
}

template <typename T>
void syncBlock_unregister_callback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->syncBlock_unRegisterCallback<T>();
}

template <typename T>
void saveBlock_unregister_callback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->saveBlock_unRegisterCallback<T>();
}
template <typename T>
void block_unregister_callback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->block_unRegisterCallback<T>();
}

#endif

