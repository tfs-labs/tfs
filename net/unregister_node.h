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

    bool Register(std::map<uint32_t, Node> node_map);
    bool StartRegisterNode(std::map<std::string, int> &server_list);
    bool StartSyncNode();

    struct NodeCompare
    {
        bool operator()(const Node& n1, const Node& n2) const {
            return n1.base58address < n2.base58address;
        }
    };

    void getIpMap(std::map<uint64_t, std::map<Node,int, NodeCompare>> & m1); 
    std::vector<Node> GetConsensusNodeList(std::vector<Node> & nodeList);
    
    void deleteConsensusNode(const std::string & base58);
    void AddConsensusNode(const std::map<Node, int, NodeCompare>  sync_node_count);
    void ClearConsensusNodeList();


private:
    friend std::string PrintCache(int where);
    std::shared_mutex _mutex_for_nodes;
    std::shared_mutex _mutex_consensus_nodes;
    std::map<std::string, Node> _nodes;

    //The IP address and the corresponding number of times are stored once and synchronized once
    std::map<uint64_t, std::map<Node,int, NodeCompare>> consensus_node_list;

};

#endif 