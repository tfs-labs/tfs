#include "node.h"

#include <memory>
#include <iostream>
#include <map>

#include <db/db_api.h>
#include "./commondata.h"
#include "./common.h"
#include "./rlp.h"
#include "./trie.h"
#include "utils/account_manager.h"
#include "include/logging.h"
#include "utils/magic_singleton.h"

std::string Trie::WapperKey(std::string str) const
{
    return str + str[str.length() - 1] + 'z';
}
bool Trie::HasTerm(std::string& s) const
{
    return s.length() > 0 && s[s.length() - 1] == 'z';
}
std::string Trie::HexToKeybytes(std::string hex)
{
    if (HasTerm(hex))
    {
        hex = hex.substr(0, hex.length() - 1);
    }
    if ((hex.length() & 1) != 0)
    {
        return "";
    }
    std::vector<char> key(hex.length() / 2);
    for (int bi = 0, ni = 0; ni < hex.length(); bi = bi + 1, ni = ni + 2)
    {
        key[bi] = hex[ni] << 4 | hex[ni + 1];
    }
    return std::string(key.begin(), key.end());
}
int Trie::PrefixLen(std::string a, std::string b) {
    int i = 0;
    int length = a.length();
    if (b.length() < length)
    {
        length = b.length();
    }
    for (; i < length; i++) {
        if (a[i] != b[i])
        {
            break;
        }
    }
    return i;
}
int Trie::Toint(char c) const
{
    if (c >= '0' && c <= '9') return c - 48;

    return c - 87;
}
void Trie::GetBlockStorage(std::pair<std::string, std::string>& rootHash, std::map<std::string, std::string>& dirtyHash)
{
    if(root == NULL) return ;
    auto hashnode = root->ToSonClass<HashNode>();
    if(this->dirtyHash.empty())
    {
        rootHash.first = hashnode->data;
        rootHash.second = "";
    }
    else
    {
        auto it = this->dirtyHash.find(hashnode->data);
        rootHash.first = it->first;
        rootHash.second = it->second;
        if (it != this->dirtyHash.end())
        {
            this->dirtyHash.erase(it);
        }
        dirtyHash = this->dirtyHash;
    }
    return;
}

nodePtr Trie::ResolveHash(nodePtr n, std::string prefix) const
{
    std::string strSha1;
    auto v = n->ToSonClass<HashNode>();

    return DescendKey(v->data);
}
ReturnNode Trie::Get(nodePtr n, std::string key, int pos) const
{
    if (n == NULL)
    {
        return ReturnNode{NULL, NULL};
    }
    else if (n->name == typeid(ValueNode).name())
    {
        return ReturnNode{ n, n };
    }
    else if (n->name == typeid(ShortNode).name())
    {
        auto sn = n->ToSonClass<ShortNode>();

        if (key.length() - pos < sn->nodeKey.length() || !(sn->nodeKey == key.substr(pos, sn->nodeKey.length())))
        {
            return ReturnNode{ NULL, NULL };
        }
        ReturnNode r = Get(sn->nodeVal, key, pos + sn->nodeKey.length());
        if (r.newNode != NULL)
        {
            sn->nodeVal = r.newNode;
        }
        return ReturnNode{r.valueNode, n};
    }
    else if (n->name == typeid(FullNode).name())
    {
        auto fn = n->ToSonClass<FullNode>();
        ReturnNode r = Get(fn->children[Toint(key[pos])], key, pos + 1);
        if (r.newNode != NULL)
        {
            fn->children[Toint(key[pos])] = r.newNode;
        }
        return ReturnNode{ r.valueNode, n };
    }
    else if (n->name == typeid(HashNode).name())
    {
        auto hashnode = n->ToSonClass<HashNode>();
        nodePtr child = ResolveHash(n, key.substr(0, pos));
        ReturnNode r = Get(child, key, pos);
        return r;
    }
    return ReturnNode{ NULL, NULL };
}

std::string Trie::Get(std::string& key) const
{
    ReturnNode r = Get(root, WapperKey(key), 0);
    if(r.valueNode != NULL)
    {
        this->root = r.newNode;
        auto vn = r.valueNode->ToSonClass<ValueNode>();
        return vn->data;
    }
    return "";
}
ReturnVal Trie::Insert(nodePtr n, std::string prefix, std::string key, nodePtr value)
{
    if (n == NULL)
    {
        return ReturnVal{ true, std::shared_ptr<packing<ShortNode>>(
                new packing<ShortNode>(ShortNode{key, value, newFlag()})), 0 };
    }
    else if (n->name == typeid(ShortNode).name())
    {
        auto sn = n->ToSonClass<ShortNode>();
        int matchlen = PrefixLen(key, sn->nodeKey);

        if (matchlen == sn->nodeKey.length())
        {
            ReturnVal r = Insert(sn->nodeVal, prefix + key.substr(0, matchlen), key.substr(matchlen), value);
            if (!r.dirty || r.err != 0)
            {
                return ReturnVal{ false, n, r.err };
            }
            return ReturnVal{ true,
            std::shared_ptr<packing<ShortNode>>(
                new packing<ShortNode>(ShortNode{sn->nodeKey, r.node, newFlag()})), 0 };
        }
        FullNode fn;
        fn.flags = newFlag();

        ReturnVal r = Insert(0, prefix + sn->nodeKey.substr(0, matchlen + 1), sn->nodeKey.substr(matchlen + 1), sn->nodeVal);
        auto ssn = r.node->ToSonClass<ShortNode>();
        fn.children[Toint(sn->nodeKey[matchlen])] = r.node;
        if (r.err != 0)
        {
            return ReturnVal{ false, 0, r.err };
        }
        ReturnVal r1 = Insert(0, prefix + key.substr(0, matchlen + 1), key.substr(matchlen + 1), value);

        fn.children[Toint(key[matchlen])] = r1.node;
        if (r1.err != 0)
        {
            return ReturnVal{ false, 0, r.err };
        }
        auto branch = std::shared_ptr<packing<FullNode>>(
            new packing<FullNode>(fn));
        // Replace this ShortNode with the branch if it occurs at index 0.
        if (matchlen == 0)
        {
            return ReturnVal{ true, branch, 0 };
        }


        return ReturnVal{ true, std::shared_ptr<packing<ShortNode>>(
                new packing<ShortNode>(ShortNode{sn->nodeKey.substr(0,matchlen), branch, newFlag()})), 0 };
    }
    else if (n->name == typeid(FullNode).name())
    {
        auto fn = n->ToSonClass<FullNode>();
        ReturnVal r = Insert(fn->children[Toint(key[0])], prefix + key[0], key.substr(1), value);
        if (!r.dirty || r.err != 0)
        {
            return ReturnVal{ false, n, r.err };
        }
        fn->flags = newFlag();
        fn->children[Toint(key[0])] = r.node;
        return ReturnVal{ true, n, 0 };
    }
    else if (n->name == typeid(HashNode).name())
    {
        auto rn = ResolveHash(n, prefix);

        ReturnVal r = Insert(rn, prefix, key, value);
        if (!r.dirty || r.err != 0)
        {
            return ReturnVal{ false, rn, r.err };
        }
        return ReturnVal{ true, r.node, 0 };

    }
    if (key.length() == 0)
    {
        int a = 1;
        auto va = n->ToSonClass<ValueNode>();
        auto vb = value->ToSonClass<ValueNode>();
        if (va->data == vb->data)
        {
            return ReturnVal{ false,value,0 };
        }
        return ReturnVal{ true,value,0 };//true, value, nil
    }
    return ReturnVal{ true,NULL,0 };
}
nodePtr Trie::Update(std::string key, std::string value)
{
    std::string k = WapperKey(key);
    if (value.length() != 0)
    {
        auto vn = std::shared_ptr<packing<ValueNode>>(
            new packing<ValueNode>(ValueNode{ value }));
        ReturnVal r = Insert(this->root, "", k, vn);
        this->root = r.node;
    }
    return NULL;
}

nodePtr Trie::DescendKey(std::string key) const
{
    DBReader dataReader;
    std::string value;

    if(this->contractDataCache != nullptr)
    {
        this->contractDataCache->get(contractAddr + "_" + key, value);
    }
    
    if(value.empty() && dataReader.GetMptValueByMptKey(contractAddr + "_" + key, value) != 0)
    {
        ERRORLOG("GetContractStorageByKey error");
    }
    dev::bytes bs = dev::fromHex(value);
    if (value == "") return NULL;
    dev::RLP r = dev::RLP(bs);
    return DecodeNode(key, r);  // if not, it must be a list
}
nodePtr Trie::DecodeShort(std::string hash, dev::RLP const& r) const
{
    NodeFlag flag;
    flag.hash = hash;
    std::string kay = r[0].toString();
    if (!HasTerm(kay))
    {
        auto v = DecodeRef(r[1]);

        return std::shared_ptr<packing<ShortNode>>(
            new packing<ShortNode>(ShortNode{ r[0].toString(),v, flag }));
    }
    else
    {

        auto v = std::shared_ptr<packing<ValueNode>>(
            new packing<ValueNode>(ValueNode{ r[1][0].toString() }));

        return std::shared_ptr<packing<ShortNode>>(
            new packing<ShortNode>(ShortNode{ r[0].toString(),v, flag }));
    };
}
nodePtr Trie::DecodeFull(std::string hash, dev::RLP const& r) const
{
    FullNode fn;
    NodeFlag flag;
    flag.hash = hash;
    fn.flags = flag;

    for (unsigned i = 0; i < 16; ++i)
    {
        if (!r[i].isEmpty())// 16 branches are allowed to be empty
        {
            auto v = DecodeRef(r[i]);
            fn.children[i] = v;
        }
    }

    return std::shared_ptr<packing<FullNode>>(
        new packing<FullNode>(fn));
}
nodePtr Trie::DecodeRef(dev::RLP const& r) const
{
    int len = r.size();
    bool a = r.isData();
    if (r.isData() && r.size() == 0)
    {
        return NULL;
    }

    else if (r.isData() && r.size() == 66)
    {
        return std::shared_ptr<packing<HashNode>>(
            new packing<HashNode>(HashNode{ r[0].toString() }));
    }
    else if (r.isList())
    {
        return DecodeNode("", r);
    }
    return NULL;
}
nodePtr Trie::DecodeNode(std::string hash, dev::RLP const& r) const
{
    if (r.isList() && r.itemCount() == 2)
    {
        return DecodeShort(hash, r);
    }
    else if (r.isList() && r.itemCount() == 17)
    {
        return DecodeFull(hash, r);
    }
    return NULL;
}

nodePtr Trie::hash(nodePtr n)
{
    if (n->name == typeid(ShortNode).name())
    {
        auto sn = n->ToSonClass<ShortNode>();

        if (!sn->nodeFlags.hash.data.empty())
        {
            return std::shared_ptr<packing<HashNode>>(
                new packing<HashNode>(sn->nodeFlags.hash));
        }

        auto hashed = HashShortNodeChildren(n);
        auto hashnode = hashed->ToSonClass<HashNode>();
        sn->nodeFlags.hash = *hashnode;

        return hashed;

    }
    else if (n->name == typeid(FullNode).name())
    {
        auto fn = n->ToSonClass<FullNode>();

        if (!fn->flags.hash.data.empty())
        {
            return std::shared_ptr<packing<HashNode>>(
                new packing<HashNode>(fn->flags.hash));
        }

        auto hashed = HashFullNodeChildren(n);
        auto hashnode = hashed->ToSonClass<HashNode>();
        fn->flags.hash = *hashnode;

        return hashed;

    }
    else
    {
        return n;
    }

}
nodePtr Trie::HashShortNodeChildren(nodePtr n)
{

    auto sn = n->ToSonClass<ShortNode>();

    auto vn = sn->nodeVal;

    if (vn->name == typeid(ShortNode).name() || vn->name == typeid(FullNode).name())
    {

        sn->nodeFlags.hash = *hash(vn)->ToSonClass<HashNode>();
    }

    return ToHash(n);
}
nodePtr Trie::HashFullNodeChildren(nodePtr n)
{

    auto fn = n->ToSonClass<FullNode>();

    FullNode collapsed;
    for (int i = 0; i < 16; i++)
    {
        auto child = fn->children[i];
        if (child != NULL)
        {
            collapsed.children[i] = hash(child);
        }
        else {
            collapsed.children[i] = NULL;
        }
    }

    return ToHash(std::shared_ptr<packing<FullNode>>(
        new packing<FullNode>(collapsed)));
}
nodePtr Trie::ToHash(nodePtr n)
{
    dev::RLPStream rlp = Encode(n);
    std::string strSha256;
    dev::bytes data = rlp.out();

    std::string stringData = dev::toHex(data);
    strSha256 = Getsha256hash(stringData);
    
    HashNode hashnode;
    hashnode.data = strSha256;
    return std::shared_ptr<packing<HashNode>>(
        new packing<HashNode>(hashnode));
}
dev::RLPStream Trie::Encode(nodePtr n)
{
    if (n->name == typeid(ShortNode).name())
    {
        dev::RLPStream rlp(2);
        auto sn = n->ToSonClass<ShortNode>();
        rlp.append(sn->nodeKey);
        rlp.append(Encode(sn->nodeVal).out());
        return rlp;
    }
    else if (n->name == typeid(FullNode).name())
    {
        dev::RLPStream rlp(17);
        auto fn = n->ToSonClass<FullNode>();
        for (auto c : fn->children)
        {
            if (c != NULL)
            {
                rlp.append(Encode(c).out());
            }
            else
            {
                rlp << "";
            }
        }
        return rlp;
    }
    else if (n->name == typeid(ValueNode).name())
    {
        dev::RLPStream rlp;
        auto vn = n->ToSonClass<ValueNode>();

        rlp << vn->data;
        return rlp;
    }
    else if (n->name == typeid(HashNode).name())
    {
        dev::RLPStream rlp;
        auto hashnode = n->ToSonClass<HashNode>();

        rlp << hashnode->data;
        return rlp;
    }
    return dev::RLPStream();
}

nodePtr Trie::Store(nodePtr n) {

    if (n->name != typeid(ShortNode).name() && n->name != typeid(FullNode).name())
    {
        return n;
    }
    else
    {
        HashNode hash;
        if (n->name == typeid(ShortNode).name())
        {
            auto sn = n->ToSonClass<ShortNode>();
            hash = sn->nodeFlags.hash;
        }
        else if(n->name == typeid(FullNode).name())
        {
            auto fn = n->ToSonClass<FullNode>();
            hash = fn->flags.hash;
        }
        // No leaf-callback used, but there's still a database. Do serial
        // insertion
        dev::RLPStream rlp = Encode(n);
        std::string strSha1;
        dev::bytes data = rlp.out();
        std::string stringData = dev::toHex(data);

        dirtyHash[hash.data] = stringData;

        return std::shared_ptr<packing<HashNode>>(
            new packing<HashNode>(hash));
    }

}
nodePtr Trie::Commit(nodePtr n)
{
    if (n->name == typeid(ShortNode).name())
    {
        auto sn = n->ToSonClass<ShortNode>();
        if (!sn->nodeFlags.dirty && !sn->nodeFlags.hash.data.empty())
        {
            return std::shared_ptr<packing<HashNode>>(
                new packing<HashNode>(sn->nodeFlags.hash));
        }

        auto vn = sn->nodeVal;
        if (vn->name == typeid(FullNode).name())
        {
            auto childV = Commit(vn);
            sn->nodeVal = childV;
        }
        auto hashed = Store(n);
        if (hashed->name == typeid(HashNode).name())
        {
            return hashed;
        }
        return n;
    }
    else if (n->name == typeid(FullNode).name())
    {
        auto fn = n->ToSonClass<FullNode>();
        if (!fn->flags.dirty && !fn->flags.hash.data.empty())
        {
            return std::shared_ptr<packing<HashNode>>(
                new packing<HashNode>(fn->flags.hash));
        }
        std::array<nodePtr, 17> hashedKids = commitChildren(n);
        fn->children = hashedKids;
        auto hashed = Store(n);
        if (hashed->name == typeid(HashNode).name())
        {
            return hashed;
        }
        return n;
    }
    else if (n->name == typeid(HashNode).name())
    {
        return n;
    }
    return NULL;
}
std::array<nodePtr, 17> Trie::commitChildren(nodePtr n)
{
    auto fn = n->ToSonClass<FullNode>();
    std::array<nodePtr, 17> children;
    for (int i = 0; i < 16; i++)
    {
        auto child = fn->children[i];
        if (child == NULL)
        {
            continue;
        }
        if (child->name == typeid(HashNode).name())
        {
            children[i] = child;
            continue;
        }

        auto hashed = Commit(child);
        children[i] = hashed;
    }
    if (fn->children[16] != NULL)
    {
        children[16] = fn->children[16];
    }
    return children;
}

void Trie::Save()
{
    if(root == NULL)
    {
        return;
    }
    hash(root);

    this->root = Commit(root);
}
