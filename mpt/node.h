/**
 * *****************************************************************************
 * @file        node.h
 * @brief       
 * @author  ()
 * @date        2023-09-28
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef TFS_MPT_NODE_H_
#define TFS_MPT_NODE_H_

#include <iostream>
#include <vector>
#include <cstddef> 
#include <memory>
#include <array>

class HashNode
{
public:
	HashNode() {}
	HashNode(std::string mdata) :data(mdata) {}
	~HashNode() {}

public:
	std::string data;
};

class ValueNode
{
public:
	ValueNode() {}
	ValueNode(std::string mdata) :data(mdata) {}
	~ValueNode() {}

public:
	std::string data;
};

class NodeFlag
{
public:
	NodeFlag() {}
	~NodeFlag() {}
	HashNode hash;
	bool dirty{false};
};

template<class T> class packing;
class Object
{
public:
	Object() {}
	~Object() {}
	template <typename T> T* ToSonClass() 
	{
		if (typeid(packing<T>).hash_code() != hashCode) {
			return nullptr;
		}

		return &(((packing<T>*)this)->data);
	}
public:
	size_t hashCode;
	std::string name;
};
typedef std::shared_ptr<Object> nodePtr;

template<class T>
class packing :public Object
{
public:
	packing(T inputData) {
		data = inputData;
		hashCode = typeid(*this).hash_code();
		name = typeid(T).name();
	}
	~packing() {}
public:
	T data;
};

class FullNode
{
public:
	FullNode() {}
	~FullNode() {}
	std::array<nodePtr, 17> children;
	NodeFlag flags;
};

class ShortNode
{
public:
	ShortNode() {}
	ShortNode(std::string key, nodePtr Val, NodeFlag flags) :nodeKey(key), nodeVal(Val), nodeFlags(flags) {}
	~ShortNode() {}
	std::string nodeKey;
	nodePtr nodeVal;
	NodeFlag nodeFlags;
};

#endif
