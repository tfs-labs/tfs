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
#include <vector>
#include "utils/timer.hpp"
#include "node.hpp"
#include "utils/account_manager.h"

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

    static std::vector<Node> intersect(const std::vector<Node>& vec1, const std::vector<Node>& vec2)
    {
        std::vector<Node> result;

        std::vector<Node> sortedVec1 = vec1;
        std::vector<Node> sortedVec2 = vec2;
        std::sort(sortedVec1.begin(), sortedVec1.end(),NodeCompare());
        std::sort(sortedVec2.begin(), sortedVec2.end(),NodeCompare());

        std::set_intersection(sortedVec1.begin(), sortedVec1.end(),
                              sortedVec2.begin(), sortedVec2.end(),
                              std::back_inserter(result),NodeCompare());

        return result;
    }


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

    static bool compareStructs(const Node& x1, const Node& x2) {
    return (x1.base58Address == x2.base58Address);
    }

    /**
     * @brief       Get the Ip Map object
     * 
     * @param       m1 
     */
    void GetIpMap(std::map<uint64_t, std::map<Node,int, NodeCompare>> & m1); 
    void GetIpMap(std::map<uint64_t, std::map<std::string, int>> & m1,std::map<uint64_t, std::map<std::string, int>> & m2);
    
    /**
     * @brief       Get the Consensus Node List object
     * 
     * @param       tergetNodeAddr 
     * @param       conNodeList 
     */
    

    /**
     * @brief       
     * 
     * @param       base58 
     */
    void DeleteSpiltNodeList(const std::string & base58);

    /**
     * @brief       
     * 
     * @param       syncNodeCount 
     */

    /**
     * @brief       
     * 
     */

    int verifyVrfDataSource(const std::vector<Node>& vrfNodelist, const uint64_t& vrfTxHeight);
    void splitAndInsertData(const std::map<Node, int, NodeCompare>  syncNodeCount);
    void ClearSplitNodeListData();

    void GetConsensusStakeNodelist(std::map<std::string,int>& consensusStakeNodeMap);
    void GetConsensusNodelist(std::map<std::string,int>& consensusNodeMap);
private:
    friend std::string PrintCache(int where);
    std::shared_mutex _mutexForNodes;
    std::shared_mutex _mutexConsensusNodes;
    std::mutex _mutexvrfNodelist;
    std::mutex _mutexStakelist;
    std::map<std::string, Node> _nodes;

    //The IP address and the corresponding number of times are stored once and synchronized once
    std::map<uint64_t, std::map<Node,int, NodeCompare>> _consensusNodeList;

    std::map<uint64_t,std::map<std::string,int>> stakeNodelist;
    std::map<uint64_t,std::map<std::string,int>> unStakeNodelist;
};

#endif 