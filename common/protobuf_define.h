#ifndef protobuf_define_h
#define protobuf_define_h

#include "../net/msg_queue.h"
#include "../protobuf/src/google/protobuf/descriptor.h"
typedef google::protobuf::Message Message;
typedef google::protobuf::Descriptor Descriptor;
typedef std::shared_ptr<Message> MessagePtr;
typedef std::function<int(const MessagePtr &, const MsgData &)> ProtoCallBack;


#endif