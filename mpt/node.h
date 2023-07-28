#ifndef TFS_MPT_NODE_H_
#define TFS_MPT_NODE_H_

#include <iostream>
#include <vector>
#include <cstddef> 
#include <memory>
#include <array>

class hashNode
{
public:
	hashNode() {}
	hashNode(std::string mdata) :data(mdata) {}
	~hashNode() {}

public:
	std::string data;
};
class valueNode
{
public:
	valueNode() {}
	valueNode(std::string mdata) :data(mdata) {}
	~valueNode() {}

public:
	std::string data;
};
class nodeFlag
{
public:
	nodeFlag() {}
	~nodeFlag() {}
	hashNode hash;
	bool dirty{false};
};

template<class T> class packing;

class object
{
public:
	object() {}
	~object() {}
	template <typename T> T* to_son_class() {
		if (typeid(packing<T>).hash_code() != hash_code) {
			return nullptr;
		}

		return &(((packing<T>*)this)->data);
	}
public:
	size_t hash_code;
	std::string name;
};
typedef std::shared_ptr<object> nodeptr;

template<class T>
class packing :public object
{
public:
	packing(T _data) {
		data = _data;
		hash_code = typeid(*this).hash_code();
		name = typeid(T).name();
	}
	~packing() {}
public:
	T data;
};

class fullNode
{
public:
	fullNode() {}
	~fullNode() {}
	std::array<nodeptr, 17> Children;
	nodeFlag flags;
};

class shortNode
{
public:
	shortNode() {}
	shortNode(std::string key, nodeptr Val, nodeFlag flags) :key_(key), Val_(Val), flags_(flags) {}
	~shortNode() {}
	std::string key_;
	nodeptr Val_;
	nodeFlag flags_;
};

#endif
