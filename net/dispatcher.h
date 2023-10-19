/**
 * *****************************************************************************
 * @file        dispatcher.h
 * @brief       
 * @author  ()
 * @date        2023-09-26
 * @copyright   tfsc
 * *****************************************************************************
 */

#ifndef _IP_DISPATCHER_H_
#define _IP_DISPATCHER_H_

#include <functional>
#include <map>

#include "./msg_queue.h"

#include "../protobuf/src/google/protobuf/descriptor.h"
#include "../utils/magic_singleton.h"

typedef google::protobuf::Message Message;
typedef google::protobuf::Descriptor Descriptor;
typedef std::shared_ptr<Message> MessagePtr;
typedef std::function<int(const MessagePtr &, const MsgData &)> ProtoCallBack;

class ProtobufDispatcher
{
public:
    /**
     * @brief       
     * 
     * @param       data 
     * @return      int 
     */
    int Handle(const MsgData &data);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */
    template <typename T>
    void CaRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */
    template <typename T>
    void NetRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb);
    
    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */
    template <typename T>
    void BroadcastRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */
    template <typename T>
    void TxRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb);
    
    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */
    template <typename T>
    void SyncBlockRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */
    template <typename T>
    void SaveBlockRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */
    template <typename T>
    void BlockRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb);

    /**
     * @brief       
     * 
     * @tparam T 
     */
    template <typename T>
    void CaUnregisterCallback();

    /**
     * @brief       
     * 
     * @tparam T 
     */
    template <typename T>
    void NetUnregisterCallback();

    /**
     * @brief       
     * 
     * @tparam T 
     */
    template <typename T>
    void BroadcastUnregisterCallback();

    /**
     * @brief       
     * 
     * @tparam T 
     */
    template <typename T>
    void TxUnregisterCallback();

    /**
     * @brief       
     * 
     * @tparam T 
     */
    template <typename T>
    void SyncBlockUnregisterCallback();

    /**
     * @brief       
     * 
     * @tparam T 
     */
    template <typename T>
    void SaveBlockUnregisterCallback();

    /**
     * @brief       
     * 
     * @tparam T 
     */
    template <typename T>
    void BlockUnregisterCallback();

    /**
     * @brief       
     * 
     */
    void RetisterAll();

    /**
     * @brief 
     * 
     */

    /**
     * @brief       
     * 
     * @param       oss 
     */
    void TaskInfo(std::ostringstream& oss);
private:
    /**
     * @brief       
     * 
     * @param       where 
     * @return      std::string 
     */
    friend std::string PrintCache(int where);

    std::map<const std::string, ProtoCallBack> _caProtocbs;
    std::map<const std::string, ProtoCallBack> _netProtocbs;
    std::map<const std::string, ProtoCallBack> _broadcastProtocbs;
    std::map<const std::string, ProtoCallBack> _txProtocbs;
    std::map<const std::string, ProtoCallBack> _syncBlockProtocbs;
    std::map<const std::string, ProtoCallBack> _saveBlockProtocbs;
    std::map<const std::string, ProtoCallBack> _blockProtocbs;
};

template <typename T>
void ProtobufDispatcher::CaRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb)
{
    _caProtocbs[T::descriptor()->name()] = [cb](const MessagePtr &msg, const MsgData &from)->int
    {
        return cb(std::static_pointer_cast<T>(msg), from);
    };
}

template <typename T>
void ProtobufDispatcher::NetRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb)
{
    _netProtocbs[T::descriptor()->name()] = [cb](const MessagePtr &msg, const MsgData &from)->int
    {
        return cb(std::static_pointer_cast<T>(msg), from);
    };
}


template <typename T>
void ProtobufDispatcher::BroadcastRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb)
{
    _broadcastProtocbs[T::descriptor()->name()] = [cb](const MessagePtr &msg, const MsgData &from)->int
    {
        return cb(std::static_pointer_cast<T>(msg), from);
    };
}

template <typename T>
void ProtobufDispatcher::TxRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb)
{
    _txProtocbs[T::descriptor()->name()] = [cb](const MessagePtr &msg, const MsgData &from)->int
    {
        return cb(std::static_pointer_cast<T>(msg), from);
    };
}

template <typename T>
void ProtobufDispatcher::SyncBlockRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb)
{
    _syncBlockProtocbs[T::descriptor()->name()] = [cb](const MessagePtr &msg, const MsgData &from)->int
    {
        return cb(std::static_pointer_cast<T>(msg), from);
    };
}

template <typename T>
void ProtobufDispatcher::SaveBlockRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb)
{
    _saveBlockProtocbs[T::descriptor()->name()] = [cb](const MessagePtr &msg, const MsgData &from)->int
    {
        return cb(std::static_pointer_cast<T>(msg), from);
    };
}

template <typename T>
void ProtobufDispatcher::BlockRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb)
{
    _blockProtocbs[T::descriptor()->name()] = [cb](const MessagePtr &msg, const MsgData &from)->int
    {
        return cb(std::static_pointer_cast<T>(msg), from);
    };
}

template <typename T>
void ProtobufDispatcher::CaUnregisterCallback()
{
    _caProtocbs.erase(T::descriptor()->name());
}
template <typename T>
void ProtobufDispatcher::NetUnregisterCallback()
{
    _netProtocbs.erase(T::descriptor()->name());
}

template <typename T>
void ProtobufDispatcher::BroadcastUnregisterCallback()
{
    _broadcastProtocbs.erase(T::descriptor()->name());
}

template <typename T>
void ProtobufDispatcher::TxUnregisterCallback()
{
    _txProtocbs.erase(T::descriptor()->name());
}

template <typename T>
void ProtobufDispatcher::SyncBlockUnregisterCallback()
{
    _syncBlockProtocbs.erase(T::descriptor()->name());
}

template <typename T>
void ProtobufDispatcher::SaveBlockUnregisterCallback()
{
    _saveBlockProtocbs.erase(T::descriptor()->name());
}

template <typename T>
void ProtobufDispatcher::BlockUnregisterCallback()
{
    _blockProtocbs.erase(T::descriptor()->name());
}
#endif
