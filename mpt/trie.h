#ifndef TFS_MPT_TRIE_H_
#define TFS_MPT_TRIE_H_

#include "node.h"
#include <memory>
#include <iostream>
#include <map>
#include "Common.h"
#include "RLP.h"
#include <boost/algorithm/hex.hpp>
#include <boost/uuid/detail/sha1.hpp>

struct ret {
public:
    bool dirty;
    nodeptr node;
    int err;
};
struct ret2 {
public:
    nodeptr value_node;
    nodeptr new_node;
};
class virtualDB
{
public:
    static std::map<std::string, std::string> db;
public:
    static void insert(std::string k, std::string v) { db[k] = v; }
    static std::string get(std::string k) 
    { 
        if (db.find(k) != db.end())
        {
            return db.at(k);
        }
        return "";
    }
    static void Getdb(std::map<std::string, std::string>& mdb)
    {
        mdb = db;
    }
    static std::map<std::string, std::string> Createdb()
    {
        std::map<std::string, std::string> Tempdb;
        return Tempdb;
    }
};

class trie
{
public:
    trie() 
    {
        root = NULL;
    }
    trie(std::string ContractAddr) 
    {
        root = NULL;
        this->mContractAddr = ContractAddr;
    }
    trie(std::string roothash, std::string ContractAddr) 
    {
        this->mContractAddr = ContractAddr;
        auto roothashnode = std::shared_ptr<packing<hashNode>>(
            new packing<hashNode>(hashNode{ roothash }));
        root = resolveHash(roothashnode, "");
    }

    nodeFlag newFlag()
    {
        nodeFlag nf;
        nf.dirty = true;
        return nf;
    }
    nodeptr resolveHash(nodeptr n, std::string prefix) const;
    std::string Get(std::string& key) const;
    ret2 Get(nodeptr n, std::string key, int pos) const;

    ret insert(nodeptr n, std::string prefix, std::string key, nodeptr value);

    nodeptr Update(std::string key, std::string value);

    nodeptr descendKey(std::string key) const;
    nodeptr decodeShort(std::string hash, dev::RLP const& _r) const;
    nodeptr decodeFull(std::string hash, dev::RLP const& _r) const;
    nodeptr decodeRef(dev::RLP const& _r) const;
    nodeptr decodeNode(std::string hash, dev::RLP const& _r) const;

    nodeptr hash(nodeptr n);
    nodeptr hashShortNodeChildren(nodeptr n);
    nodeptr hashFullNodeChildren(nodeptr n);
    nodeptr ToHash(nodeptr n);
    dev::RLPStream encode(nodeptr n);

    nodeptr store(nodeptr n);
    nodeptr commit(nodeptr n);
    std::array<nodeptr, 17>commitChildren(nodeptr n);

    void save();

    std::string WapperKey(std::string str) const;
    bool hasTerm(std::string& s) const;
    std::string hexToKeybytes(std::string hex);
    int prefixLen(std::string a, std::string b);
    int toint(char c) const;

    void GetBlockStorage(std::pair<std::string, std::string>& rootHash, std::map<std::string, std::string>& dirtyhash);
public:
    mutable nodeptr root;
    std::string mContractAddr;
    std::map<std::string, std::string> dirtyhash;
};
#endif


