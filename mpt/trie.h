/**
 * *****************************************************************************
 * @file        trie.h
 * @brief       
 * @author  ()
 * @date        2023-09-28
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef TFS_MPT_TRIE_H_
#define TFS_MPT_TRIE_H_

#include <memory>
#include <iostream>
#include <map>

#include <boost/algorithm/hex.hpp>
#include <boost/uuid/detail/sha1.hpp>

#include "node.h"
#include "common.h"
#include "rlp.h"
struct ReturnVal 
{
public:
    bool dirty;
    nodePtr node;
    int err;
};
struct ReturnNode 
{
public:
    nodePtr valueNode;
    nodePtr newNode;
};
class VirtualDB
{
public:
    static std::map<std::string, std::string> db;
public:
    static void Insert(std::string k, std::string v) { db[k] = v; }
    static std::string Get(std::string k) 
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

class Trie
{
public:
    Trie() 
    {
        root = NULL;
    }
    Trie(std::string ContractAddr) 
    {
        root = NULL;
        this->contractAddr = ContractAddr;
    }
    Trie(std::string roothash, std::string ContractAddr) 
    {
        this->contractAddr = ContractAddr;
        auto roothashnode = std::shared_ptr<packing<HashNode>>(
            new packing<HashNode>(HashNode{ roothash }));
        root = ResolveHash(roothashnode, "");
    }

    NodeFlag newFlag()
    {
        NodeFlag nf;
        nf.dirty = true;
        return nf;
    }
    nodePtr ResolveHash(nodePtr n, std::string prefix) const;
    std::string Get(std::string& key) const;
    ReturnNode Get(nodePtr n, std::string key, int pos) const;

    ReturnVal Insert(nodePtr n, std::string prefix, std::string key, nodePtr value);

    nodePtr Update(std::string key, std::string value);

    nodePtr DescendKey(std::string key) const;
    nodePtr DecodeShort(std::string hash, dev::RLP const& r) const;
    nodePtr DecodeFull(std::string hash, dev::RLP const& r) const;
    nodePtr DecodeRef(dev::RLP const& r) const;
    nodePtr DecodeNode(std::string hash, dev::RLP const& r) const;

    nodePtr hash(nodePtr n);
    nodePtr HashShortNodeChildren(nodePtr n);
    nodePtr HashFullNodeChildren(nodePtr n);
    nodePtr ToHash(nodePtr n);
    dev::RLPStream Encode(nodePtr n);

    nodePtr Store(nodePtr n);
    nodePtr Commit(nodePtr n);
    std::array<nodePtr, 17>commitChildren(nodePtr n);

    void Save();

    std::string WapperKey(std::string str) const;
    bool HasTerm(std::string& s) const;
    std::string HexToKeybytes(std::string hex);
    int PrefixLen(std::string a, std::string b);
    int Toint(char c) const;

    void GetBlockStorage(std::pair<std::string, std::string>& rootHash, std::map<std::string, std::string>& dirtyHash);
public:
    mutable nodePtr root;
    std::string contractAddr;
    std::map<std::string, std::string> dirtyHash;
};
#endif


