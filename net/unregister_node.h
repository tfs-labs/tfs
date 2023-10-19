/**
 * *****************************************************************************
 * @file        unregister_node.h
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _UnregisterNode_H_
#define _UnregisterNode_H_

#include <shared_mutex>
#include <map>
#include <set>
#include "utils/timer.hpp"
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
    /**
     * @brief       
     * 
     * @param       node 
     * @return      int 
     */
    int Add(const Node & node);
    /**
     * @brief       
     * 
     * @param       node 
     * @return      int 
     */
    int Find(const Node & node);

    /**
     * @brief       
     * 
     * @param       nodeMap 
     * @return      true 
     * @return      false 
     */
    bool Register(std::map<uint32_t, Node> nodeMap);
    
    /**
     * @brief       
     * 
     * @param       serverList 
     * @return      true 
     * @return      false 
     */
    bool StartRegisterNode(std::map<std::string, int> &serverList);

    /**
     * @brief       
     * 
     * @return      true 
     * @return      false 
     */
    bool StartSyncNode();

    struct NodeCompare
    {
        bool operator()(const Node& n1, const Node& n2) const {
            return n1.base58Address < n2.base58Address;
        }
    };

    /**
     * @brief       Get the Ip Map object
     * 
     * @param       m1 
     */
    void GetIpMap(std::map<uint64_t, std::map<Node,int, NodeCompare>> & m1); 
    
    /**
     * @brief       Get the Consensus Node List object
     * 
     * @param       nodeList 
     * @return      std::vector<Node> 
     */
    std::vector<Node> GetConsensusNodeList(std::vector<Node> & nodeList);
    
    /**
     * @brief       
     * 
     * @param       base58 
     */
    void DeleteConsensusNode(const std::string & base58);

    /**
     * @brief       
     * 
     * @param       syncNodeCount 
     */
    void AddConsensusNode(const std::map<Node, int, NodeCompare>  syncNodeCount);

    /**
     * @brief       
     * 
     */
    void ClearConsensusNodeList();


private:
    friend std::string PrintCache(int where);
    std::shared_mutex _mutexForNodes;
    std::shared_mutex _mutexConsensusNodes;
    std::map<std::string, Node> _nodes;

    //The IP address and the corresponding number of times are stored once and synchronized once
    std::map<uint64_t, std::map<Node,int, NodeCompare>> _consensusNodeList;

};

#endif 