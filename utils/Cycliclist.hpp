
#ifndef _CYCLICLIST_HPP_

#define _CYCLICLIST_HPP_
#include <utility>
#include <memory>
#include <functional>
#include <vector>





template <typename T>
class ListIter
{

public:
	using value_type = T;
	using reference = T&;
	using const_referenct = const T&;
	using pointer = T*;
	using const_pointor = const T*;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	ListIter(pointer p = nullptr) : Iter(p) {}

	bool operator==(const ListIter& rhs) const noexcept
	{
		return Iter == rhs.Iter;
	}
	bool operator!=(const ListIter& rhs) const noexcept
	{
		return Iter != rhs.Iter;
	}
	ListIter& operator++(int)
	{
        if(Iter==nullptr){
            return *this;
        }
		Iter = Iter->next();
		return *this;
	}
	ListIter& operator--(int) {
        if(Iter==nullptr){
            return *this;
        }
		Iter = Iter->prev();
		return *this;
	}
	ListIter operator+(int value) {

		pointer newPointer = Iter;
        if(Iter==nullptr){
            return ListIter<T>(nullptr);
        }
		if (value > 0) {
			for (int i = 0; i < value; i++) {
				newPointer = newPointer->next();
				
			}
			
		}
		else if(value <0) {
			for (int i = 0; i > value; i--) {
				newPointer = newPointer->prev();
				
			}
		}
		return ListIter<T>(newPointer);
	}
	ListIter operator-(int value) {
		pointer newPointer = Iter;
        if(Iter==nullptr){
            return ListIter<T>(nullptr);
        }

		if (value > 0) {
			for (int i = 0; i < value; i++) {
				newPointer = newPointer->prev();
				
			}

		}
		else if (value < 0) {
			for (int i = 0; i > value; i--) {
				newPointer = newPointer->next();
				
			}
		}
		return ListIter<T>(newPointer);
	}
	/*ListIter& operator++(int)
	{
		value_type tmp = *this;
		++&* this;
		return tmp;
	}*/

	reference operator*()
	{
		return *Iter;
	}
	pointer getPtr() {
		return Iter;
	}
	pointer operator->()
	{
		return Iter;
	}
private:
	pointer Iter=nullptr;
};


template <typename T>
class Cycliclist {
public:
	template<typename T_>
	class Node {
	public:
		Node(const T_& value) { data = value; }
		T_ data;
		Node<T_>* next() { return next_; }
		Node<T_>* prev() { return pre; }
	private:
		friend class Cycliclist<T_>;



		Node<T_>* pre = nullptr;
		Node<T_>* next_ = nullptr;
	};
	using iterator = ListIter<Node<T>>;
	Cycliclist() {}

	~Cycliclist(){
		clear();
	}

	Cycliclist(Cycliclist& list) {
		
	}

	Cycliclist(Cycliclist&& list) {
		m_size = list.m_size;
		head = list.head;
		last = list.last;
	}

	iterator next(iterator ite) {
		return ite->next();
	}

	iterator begin() {
		return iterator(head);
	}

	iterator end() {
		return iterator(last);
	}

	int size() {
		return m_size;
	}

	bool isEmpty() {
		if (m_size == 0) {
			return true;
		}
		return false;
	}

	void push_front(const T& value) {
		if (head == nullptr) {

			Node<T>* node = new Node<T>(value);
			head = node;
			node->next_ = node;
			last = node;
			node->pre = node;
		}
		else {
			Node<T>* temp_head = nullptr;
			Node<T>* node = new Node<T>(value);
			temp_head = head;
			head = node;
			node->next_ = temp_head;
			node->pre = temp_head->pre;
			temp_head->pre = node;
			last->next_ = node;
		}
		m_size++;
	}

	void push_back(const T& value) {
		if (head == nullptr) {

			Node<T>* node = new Node<T>(value);
			head = node;
			last = node;
			node->pre = node;
			node->next_ = node;
		}
		else {
			Node<T>* temp_last = nullptr;
			Node<T>* node = new Node<T>(value);
			temp_last = last;
			node->next_ = temp_last->next_;
			node->pre = temp_last;
			temp_last->next_ = node;
			last = node;
			head->pre = node;

		}
		m_size++;
	}

	T& operator[](int index) {
		auto ite = begin();
		ite =ite+index;
		return ite->data;
	}

	std::vector<iterator> filter(std::function<bool(iterator)> lamda) {
		auto ite = this->begin();
		std::vector<iterator> res;
		for (; ite != this->end(); ite++) {
			if (lamda(ite)) {
				res.push_back(ite);
			}
		}
		if(lamda(ite)){
			res.push_back(ite);
		}
		return res;
	}

	iterator remove(iterator ite) {

		if (head == nullptr) {
			return iterator(nullptr);
		}

		Node<T>* temp_node = ite.getPtr();
		Node<T>* next_node = temp_node->next_;

		if (ite.getPtr() == head) {
			head = next_node;
		}
		if (ite.getPtr() == last) {
			last = temp_node->pre;
		}

		temp_node->pre->next_ = temp_node->next_;
		temp_node->next_->pre = temp_node->pre;
		delete temp_node;
		m_size--;
		if (m_size == 0) {
			return iterator(nullptr);
		}
		return iterator(next_node);
	}

	iterator remove(std::function<bool(iterator)> lamda) {

		auto ite = this->begin();
		for (; ite != this->end(); ite++) {
			if (lamda(ite)) {
				Node<T>* next_node = ite->next();
				remove(ite);
				return iterator(next_node);
				break;
			}
		}
		if(lamda(ite)){
			Node<T>* next_node = ite->next();
				remove(ite);
				return iterator(next_node);
		}

		return iterator(nullptr);
	}


	void clear() {
		if(head == nullptr)
		{
			return;
		}
		Node<T>* nextNode = head->next();
		head->next_ = nullptr;
		while (nextNode != nullptr) {
			Node<T>* delete_node = nextNode;
			nextNode = nextNode->next();
			delete delete_node;
		}
	}
private:
	Node<T>* head = nullptr;
	Node<T>* last = nullptr;
	int m_size = 0;
};


#endif // !_CYCLICLIST_HPP_
