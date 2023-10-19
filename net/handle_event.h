/**
 * *****************************************************************************
 * @file        handle_event.h
 * @brief       
 * @author  ()
 * @date        2023-09-26
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _HANDLE_EVENT_H_
#define _HANDLE_EVENT_H_

#include <memory>

#include "./msg_queue.h"

#include "../common/task_pool.h"
#include "../proto/net.pb.h"
#include "../proto/ca_protomsg.pb.h"
#include "../proto/interface.pb.h"

/**
 * @brief       
 * 
 * @param       printMsgReq 
 * @param       from 
 * @return      int 
 */
int HandlePrintMsgReq(const std::shared_ptr<PrintMsgReq> &printMsgReq, const MsgData &from);

/**
 * @brief       
 * 
 * @param       nodeinfo 
 * @param       fromIp 
 * @param       fromPort 
 * @return      int 
 */

/**
 * @brief       
 * 
 * @param       nodeinfo 
 * @param       fromIp 
 * @param       fromPort 
 * @return      int 
 */
int VerifyRegisterNode(const NodeInfo &nodeinfo, uint32_t &fromIp, uint32_t &fromPort);

/**
 * @brief       
 * 
 * @param       broadcastMsgReq 
 * @param       from 
 * @return      int 
 */
int HandleBroadcastMsgReq(const std::shared_ptr<BroadcastMsgReq> &broadcastMsgReq, const MsgData &from);

/**
 * @brief       
 * 
 * @param       msg 
 * @param       msgData 
 * @return      int 
 */
int HandleBroadcastMsg( const std::shared_ptr<BuildBlockBroadcastMsg>& msg, const MsgData& msgData);


/**
 * @brief       
 * 
 * @param       registerNode 
 * @param       from 
 * @return      int 
 */
int HandleRegisterNodeReq(const std::shared_ptr<RegisterNodeReq> &registerNode, const MsgData &from);
/**
 * @brief       
 * 
 * @param       registerNodeAck 
 * @param       from 
 * @return      int 
 */
int HandleRegisterNodeAck(const std::shared_ptr<RegisterNodeAck> &registerNodeAck, const MsgData &from);

/**
 * @brief       
 * 
 * @param       pingReq 
 * @param       from 
 * @return      int 
 */
int HandlePingReq(const std::shared_ptr<PingReq> &pingReq, const MsgData &from);
/**
 * @brief       
 * 
 * @param       pongReq 
 * @param       from 
 * @return      int 
 */
int HandlePongReq(const std::shared_ptr<PongReq> &pongReq, const MsgData &from);

/**
 * @brief       
 * 
 * @param       syncNodeReq 
 * @param       from 
 * @return      int 
 */
int HandleSyncNodeReq(const std::shared_ptr<SyncNodeReq> &syncNodeReq, const MsgData &from);
/**
 * @brief       
 * 
 * @param       syncNodeAck 
 * @param       from 
 * @return      int 
 */
int HandleSyncNodeAck(const std::shared_ptr<SyncNodeAck> &syncNodeAck, const MsgData &from);

/**
 * @brief       
 * 
 * @param       echoReq 
 * @param       from 
 * @return      int 
 */
int HandleEchoReq(const std::shared_ptr<EchoReq> &echoReq, const MsgData &from);
/**
 * @brief       
 * 
 * @param       echoAck 
 * @param       from 
 * @return      int 
 */
int HandleEchoAck(const std::shared_ptr<EchoAck> &echoAck, const MsgData &from);

/**
 * @brief       
 * 
 * @param       testReq 
 * @param       from 
 * @return      int 
 */
int HandleNetTestReq(const std::shared_ptr<TestNetReq> &testReq, const MsgData &from);
/**
 * @brief       
 * 
 * @param       testAck 
 * @param       from 
 * @return      int 
 */
int HandleNetTestAck(const std::shared_ptr<TestNetAck> &testAck, const MsgData &from);

/**
 * @brief       
 * 
 * @param       req 
 * @param       from 
 * @return      int 
 */
int HandleCheckTxReq(const std::shared_ptr<CheckTxReq>& req, const MsgData& from);
/**
 * @brief       
 * 
 * @param       ack 
 * @param       from 
 * @return      int 
 */
int HandleCheckTxAck(const std::shared_ptr<CheckTxAck>& ack, const MsgData& from);

/**
 * @brief       
 * 
 * @param       req 
 * @param       from 
 * @return      int 
 */
int HandleNodeHeightChangedReq(const std::shared_ptr<NodeHeightChangedReq>& req, const MsgData& from);
/**
 * @brief       
 * 
 * @param       req 
 * @param       from 
 * @return      int 
 */
int HandleNodeBase58AddrChangedReq(const std::shared_ptr<NodeBase58AddrChangedReq>& req, const MsgData& from);

#endif
