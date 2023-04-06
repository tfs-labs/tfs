#ifndef _UnregisterNode_H_
#define _UnregisterNode_H_

#include <shared_mutex>
#include <map>
#include <set>
#include "utils/CTimer.hpp"
#include "node.hpp"

class UnregisterNode
{
public:
    UnregisterNode();
    UnregisterNode(UnregisterNode &&) = delete;
    UnregisterNode(const UnregisterNode &) = delete;
    UnregisterNode &operator=(UnregisterNode &&) = delete;
    UnregisterNode &operator=(const UnregisterNode &) = delete;
    ~UnregisterNode();
public:
    int Add(const Node & node);
    int Find(const Node & node);

    bool tool_connect(const std::string & ip,int port);

    bool Register(std::map<uint32_t, Node> node_map);
    bool StartRegisterNode(std::map<std::string, int> &server_list);
    bool StartSyncNode();
    
private:
    std::shared_mutex _mutex_for_nodes;
    std::map<std::string, Node> _nodes;
};

#endif 