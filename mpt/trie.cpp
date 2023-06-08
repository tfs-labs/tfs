#include "node.h"
#include <memory>
#include <iostream>
#include <map>
#include <db/db_api.h>
#include "CommonData.h"
#include "Common.h"
#include "RLP.h"
#include "trie.h"
#include "utils/AccountManager.h"
#include "include/logging.h"

std::map<std::string, std::string> virtualDB::db = virtualDB::Createdb();

std::string trie::WapperKey(std::string str) const
{
    return str + 'z';
}
bool trie::hasTerm(std::string& s) const
{
    return s.length() > 0 && s[s.length() - 1] == 'z';
}
std::string trie::hexToKeybytes(std::string hex)
{
    if (hasTerm(hex))
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
int trie::prefixLen(std::string a, std::string b) {
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
int trie::toint(char c) const
{
    if (c >= '0' && c <= '9') return c - 48;

    return c - 87;
}
void trie::GetBlockStorage(std::pair<std::string, std::string>& rootHash, std::map<std::string, std::string>& dirtyhash)
{
    if(root == NULL) return ;
    auto hn = root->to_son_class<hashNode>();
    if(this->dirtyhash.empty())
    {
        rootHash.first = hn->data;
        rootHash.second = "";
        return;
    }
    else
    {
        auto it = this->dirtyhash.find(hn->data);
        rootHash.first = it->first;
        rootHash.second = it->second;
        if (it != this->dirtyhash.end())
        {
            this->dirtyhash.erase(it);
        }
        dirtyhash = this->dirtyhash;
    }
    return;
}

nodeptr trie::resolveHash(nodeptr n, std::string prefix) const
{
    std::string str_sha1;
    auto v = n->to_son_class<hashNode>();

    return descendKey(v->data);
}
ret2 trie::Get(nodeptr n, std::string key, int pos) const
{
    if (n == NULL)
    {
        return ret2{NULL, NULL};
    }
    else if (n->name == typeid(valueNode).name())
    {
        return ret2{ n, n };
    }
    else if (n->name == typeid(shortNode).name())
    {
        auto sn = n->to_son_class<shortNode>();

        if (key.length() - pos < sn->key_.length() || !(sn->key_ == key.substr(pos, sn->key_.length())))
        {
            return ret2{ NULL, NULL };
        }
        ret2 r = Get(sn->Val_, key, pos + sn->key_.length());
        if (r.new_node != NULL)
        {
            sn->Val_ = r.new_node;
        }
        return ret2{r.value_node, n};
    }
    else if (n->name == typeid(fullNode).name())
    {
        auto fn = n->to_son_class<fullNode>();
        ret2 r = Get(fn->Children[toint(key[pos])], key, pos + 1);
        if (r.new_node != NULL)
        {
            fn->Children[toint(key[pos])] = r.new_node;
        }
        return ret2{ r.value_node, n };
    }
    else if (n->name == typeid(hashNode).name())
    {
        auto hn = n->to_son_class<hashNode>();
        nodeptr child = resolveHash(n, key.substr(0, pos));
        ret2 r = Get(child, key, pos);
        return r;
    }
    return ret2{ NULL, NULL };
}

std::string trie::Get(std::string& key) const
{
    ret2 r = Get(root, WapperKey(key), 0);
    if(r.value_node != NULL)
    {
        this->root = r.new_node;
        auto vn = r.value_node->to_son_class<valueNode>();
        return vn->data;
    }
    return "";
}
ret trie::insert(nodeptr n, std::string prefix, std::string key, nodeptr value)
{
    if (n == NULL)
    {
        return ret{ true, std::shared_ptr<packing<shortNode>>(
                new packing<shortNode>(shortNode{key, value, newFlag()})), 0 };
    }
    else if (n->name == typeid(shortNode).name())
    {
        auto sn = n->to_son_class<shortNode>();
        int matchlen = prefixLen(key, sn->key_);

        if (matchlen == sn->key_.length())
        {
            ret r = insert(sn->Val_, prefix + key.substr(0, matchlen), key.substr(matchlen), value);
            if (!r.dirty || r.err != 0)
            {
                return ret{ false, n, r.err };
            }
            return ret{ true,
            std::shared_ptr<packing<shortNode>>(
                new packing<shortNode>(shortNode{sn->key_, r.node, newFlag()})), 0 };
        }
        fullNode fn;
        fn.flags = newFlag();

        ret r = insert(0, prefix + sn->key_.substr(0, matchlen + 1), sn->key_.substr(matchlen + 1), sn->Val_);
        auto ssn = r.node->to_son_class<shortNode>();
        fn.Children[toint(sn->key_[matchlen])] = r.node;
        if (r.err != 0)
        {
            return ret{ false, 0, r.err };
        }
        ret r1 = insert(0, prefix + key.substr(0, matchlen + 1), key.substr(matchlen + 1), value);

        fn.Children[toint(key[matchlen])] = r1.node;
        if (r1.err != 0)
        {
            return ret{ false, 0, r.err };
        }
        auto branch = std::shared_ptr<packing<fullNode>>(
            new packing<fullNode>(fn));
        // Replace this shortNode with the branch if it occurs at index 0.
        if (matchlen == 0)
        {
            return ret{ true, branch, 0 };
        }


        return ret{ true, std::shared_ptr<packing<shortNode>>(
                new packing<shortNode>(shortNode{sn->key_.substr(0,matchlen), branch, newFlag()})), 0 };
    }
    else if (n->name == typeid(fullNode).name())
    {
        auto fn = n->to_son_class<fullNode>();
        ret r = insert(fn->Children[toint(key[0])], prefix + key[0], key.substr(1), value);
        if (!r.dirty || r.err != 0)
        {
            return ret{ false, n, r.err };
        }
        fn->flags = newFlag();
        fn->Children[toint(key[0])] = r.node;
        return ret{ true, n, 0 };
    }
    else if (n->name == typeid(hashNode).name())
    {
        auto rn = resolveHash(n, prefix);

        ret r = insert(rn, prefix, key, value);
        if (!r.dirty || r.err != 0)
        {
            return ret{ false, rn, r.err };
        }
        return ret{ true, r.node, 0 };

    }
    if (key.length() == 0)
    {
        int a = 1;
        auto va = n->to_son_class<valueNode>();
        auto vb = value->to_son_class<valueNode>();
        if (va->data == vb->data)
        {
            return ret{ false,value,0 };
        }
        return ret{ true,value,0 };//true, value, nil
    }
    return ret{ true,NULL,0 };
}
nodeptr trie::Update(std::string key, std::string value)
{
    std::string k = WapperKey(key);
    if (value.length() != 0)
    {
        auto vn = std::shared_ptr<packing<valueNode>>(
            new packing<valueNode>(valueNode{ value }));
        ret r = insert(this->root, "", k, vn);
        this->root = r.node;
    }
    return NULL;
}

nodeptr trie::descendKey(std::string key) const
{
    DBReader data_reader;
    std::string value;
    if(data_reader.GetMptValueByMptKey(mContractAddr + "_" + key, value) != 0)
    {
        ERRORLOG("GetContractStorageByKey error");
    }
    dev::bytes bs = dev::fromHex(value);
    if (value == "") return NULL;
    dev::RLP r = dev::RLP(bs);
    return decodeNode(key, r);  // if not, it must be a list
}
nodeptr trie::decodeShort(std::string hash, dev::RLP const& _r) const
{
    nodeFlag flag;
    flag.hash = hash;
    std::string kay = _r[0].toString();
    if (!hasTerm(kay))
    {
        auto v = decodeRef(_r[1]);

        return std::shared_ptr<packing<shortNode>>(
            new packing<shortNode>(shortNode{ _r[0].toString(),v, flag }));
    }
    else
    {

        auto v = std::shared_ptr<packing<valueNode>>(
            new packing<valueNode>(valueNode{ _r[1][0].toString() }));

        return std::shared_ptr<packing<shortNode>>(
            new packing<shortNode>(shortNode{ _r[0].toString(),v, flag }));
    };
}
nodeptr trie::decodeFull(std::string hash, dev::RLP const& _r) const
{
    fullNode fn;
    nodeFlag flag;
    flag.hash = hash;
    fn.flags = flag;

    for (unsigned i = 0; i < 16; ++i)
    {
        if (!_r[i].isEmpty())// 16 branches are allowed to be empty
        {
            auto v = decodeRef(_r[i]);
            fn.Children[i] = v;
        }
    }

    return std::shared_ptr<packing<fullNode>>(
        new packing<fullNode>(fn));
}
nodeptr trie::decodeRef(dev::RLP const& _r) const
{
    int len = _r.size();
    bool a = _r.isData();
    if (_r.isData() && _r.size() == 0)
    {
        return NULL;
    }

    else if (_r.isData() && _r.size() == 66)
    {
        return std::shared_ptr<packing<hashNode>>(
            new packing<hashNode>(hashNode{ _r[0].toString() }));
    }
    else if (_r.isList())
    {
        return decodeNode("", _r);
    }
    return NULL;
}
nodeptr trie::decodeNode(std::string hash, dev::RLP const& _r) const
{
    if (_r.isList() && _r.itemCount() == 2)
    {
        return decodeShort(hash, _r);
    }
    else if (_r.isList() && _r.itemCount() == 17)
    {
        return decodeFull(hash, _r);
    }
    return NULL;
}

nodeptr trie::hash(nodeptr n)
{
    if (n->name == typeid(shortNode).name())
    {
        auto sn = n->to_son_class<shortNode>();

        if (!sn->flags_.hash.data.empty())
        {
            return std::shared_ptr<packing<hashNode>>(
                new packing<hashNode>(sn->flags_.hash));
        }

        auto hashed = hashShortNodeChildren(n);
        auto hn = hashed->to_son_class<hashNode>();
        sn->flags_.hash = *hn;

        return hashed;

    }
    else if (n->name == typeid(fullNode).name())
    {
        auto fn = n->to_son_class<fullNode>();

        if (!fn->flags.hash.data.empty())
        {
            return std::shared_ptr<packing<hashNode>>(
                new packing<hashNode>(fn->flags.hash));
        }

        auto hashed = hashFullNodeChildren(n);
        auto hn = hashed->to_son_class<hashNode>();
        fn->flags.hash = *hn;

        return hashed;

    }
    else
    {
        return n;
    }

}
nodeptr trie::hashShortNodeChildren(nodeptr n)
{

    auto sn = n->to_son_class<shortNode>();

    auto vn = sn->Val_;

    if (vn->name == typeid(shortNode).name() || vn->name == typeid(fullNode).name())
    {

        sn->flags_.hash = *hash(vn)->to_son_class<hashNode>();
    }

    return ToHash(n);
}
nodeptr trie::hashFullNodeChildren(nodeptr n)
{

    auto fn = n->to_son_class<fullNode>();

    fullNode collapsed;
    for (int i = 0; i < 16; i++)
    {
        auto child = fn->Children[i];
        if (child != NULL)
        {
            collapsed.Children[i] = hash(child);
        }
        else {
            collapsed.Children[i] = NULL;
        }
    }

    return ToHash(std::shared_ptr<packing<fullNode>>(
        new packing<fullNode>(collapsed)));
}
nodeptr trie::ToHash(nodeptr n)
{
    dev::RLPStream rlp = encode(n);
    std::string str_sha256;
    dev::bytes data = rlp.out();

    std::string stringData = dev::toHex(data);
    str_sha256 = getsha256hash(stringData);
    
    hashNode hn;
    hn.data = str_sha256;
    return std::shared_ptr<packing<hashNode>>(
        new packing<hashNode>(hn));
}
dev::RLPStream trie::encode(nodeptr n)
{
    if (n->name == typeid(shortNode).name())
    {
        dev::RLPStream rlp(2);
        auto sn = n->to_son_class<shortNode>();
        rlp.append(sn->key_);
        rlp.append(encode(sn->Val_).out());
        return rlp;
    }
    else if (n->name == typeid(fullNode).name())
    {
        dev::RLPStream rlp(17);
        auto fn = n->to_son_class<fullNode>();
        for (auto c : fn->Children)
        {
            if (c != NULL)
            {
                rlp.append(encode(c).out());
            }
            else
            {
                rlp << "";
            }
        }
        return rlp;
    }
    else if (n->name == typeid(valueNode).name())
    {
        dev::RLPStream rlp;
        auto vn = n->to_son_class<valueNode>();

        rlp << vn->data;
        return rlp;
    }
    else if (n->name == typeid(hashNode).name())
    {
        dev::RLPStream rlp;
        auto hn = n->to_son_class<hashNode>();

        rlp << hn->data;
        return rlp;
    }
    return dev::RLPStream();
}

nodeptr trie::store(nodeptr n) {

    if (n->name != typeid(shortNode).name() && n->name != typeid(fullNode).name())
    {
        return n;
    }
    else
    {
        hashNode hash;
        if (n->name == typeid(shortNode).name())
        {
            auto sn = n->to_son_class<shortNode>();
            hash = sn->flags_.hash;
        }
        else if(n->name == typeid(fullNode).name())
        {
            auto fn = n->to_son_class<fullNode>();
            hash = fn->flags.hash;
        }
        // No leaf-callback used, but there's still a database. Do serial
        // insertion
        dev::RLPStream rlp = encode(n);
        std::string str_sha1;
        dev::bytes data = rlp.out();
        std::string stringData = dev::toHex(data);

        dirtyhash[hash.data] = stringData;

        return std::shared_ptr<packing<hashNode>>(
            new packing<hashNode>(hash));
    }

}
nodeptr trie::commit(nodeptr n)
{
    if (n->name == typeid(shortNode).name())
    {
        auto sn = n->to_son_class<shortNode>();
        if (!sn->flags_.dirty && !sn->flags_.hash.data.empty())
        {
            return std::shared_ptr<packing<hashNode>>(
                new packing<hashNode>(sn->flags_.hash));
        }

        auto vn = sn->Val_;
        if (vn->name == typeid(fullNode).name())
        {
            auto childV = commit(vn);
            sn->Val_ = childV;
        }
        auto hashed = store(n);
        if (hashed->name == typeid(hashNode).name())
        {
            return hashed;
        }
        return n;
    }
    else if (n->name == typeid(fullNode).name())
    {
        auto fn = n->to_son_class<fullNode>();
        if (!fn->flags.dirty && !fn->flags.hash.data.empty())
        {
            return std::shared_ptr<packing<hashNode>>(
                new packing<hashNode>(fn->flags.hash));
        }
        std::array<nodeptr, 17> hashedKids = commitChildren(n);
        fn->Children = hashedKids;
        auto hashed = store(n);
        if (hashed->name == typeid(hashNode).name())
        {
            return hashed;
        }
        return n;
    }
    else if (n->name == typeid(hashNode).name())
    {
        return n;
    }
    return NULL;
}
std::array<nodeptr, 17> trie::commitChildren(nodeptr n)
{
    auto fn = n->to_son_class<fullNode>();
    std::array<nodeptr, 17> children;
    for (int i = 0; i < 16; i++)
    {
        auto child = fn->Children[i];
        if (child == NULL)
        {
            continue;
        }
        if (child->name == typeid(hashNode).name())
        {
            children[i] = child;
            continue;
        }

        auto hashed = commit(child);
        children[i] = hashed;
    }
    if (fn->Children[16] != NULL)
    {
        children[16] = fn->Children[16];
    }
    return children;
}

void trie::save()
{
    if(root == NULL)
    {
        return;
    }
    hash(root);

    this->root = commit(root);
}
