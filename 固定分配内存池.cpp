#define _CRT_SECURE_NO_WARNINGS 1
#pragma once
#include<iostream>
#include<cstdlib>

using namespace std;


template<typename T>
class objectpool
{
	//首先给出内存池的节点，
	struct BlockNode
	{
		void* _memory;			//对应每个节点所挂的内存块
		BlockNode * _next;		//指向下一个节点的指针。
		size_t _objNum;			//表示内存对象的个数
		//进行内存块的初始化
		BlockNode(size_t objnum)
			: _next(NULL)
			, _objNum(objnum)
			
		{
			_memory = malloc(_itemsize*objnum);
		}
		//内存块析构
		~BlockNode()
		{
			free(_memory);
			_memory = NULL;
			_next = NULL;
			_objNum = 0;
		}
	};
public:
	//用来初始化对象大小的函数
	static size_t IninItemSize();
	//固定大小对象池大构造函数
	objectpool(size_t initNum = 32, size_t maxNum = 100000)
		:_countin(0)
		, _Maximum(maxNum)
	{
		_first = _last = new BlockNode(initNum);
		_lastDelete = NULL;
	}
	//对象池的析构
	~objectpool()
	{
		Destory();
		_first = _last = NULL;



		////当把所有的内存块进行释放以后，这个时候_lastdelete不允许还链有内存块
		//
		//if (_lastDelete==NULL)
		//	cout << "发生了内存泄漏" << endl;

	}
	void Destory()
	{
		//进行销毁内存池的时候，我们这个时候采取的方式就是释放每一个节点挂的memory。
		BlockNode* cur = _first;
		BlockNode* del = NULL;
		while (cur)
		{
			del = cur;
			cur = cur->_next;
			delete del;
		}
		
	}
	T* New()
	{
		//首先释放以前的内存地址空间
		//内存块里面进行申请
		//申请新的节点对象
		if (_lastDelete)
		{
			T* obj = _lastDelete;
			 //强转为T**，然后解引用。这里解决了32位程序和64为程序的限制，T**直接取到了正确的地址内容。
			_lastDelete = *((T**)_lastDelete);

			return new(obj)T;
		}


		//当BlockNode满了的时候，这个时候就需要去到下一个BlockNode去构造对象。
		if (_countin == _last->_objNum)
		{
			size_t newNodeNum = _last->_objNum * 2;			
			if (newNodeNum >= _Maximum)
				newNodeNum = _Maximum;
			_last->_next = new BlockNode(newNodeNum);
			if (_last->_next == NULL)
				cout << "内存开辟失败" << endl;
			_last = _last->_next; 
			_countin = 0;
		}

		//在memory上剩余的位置进行构造
		T* obj = (T*)((char*)_last->_memory + _countin*_itemsize);
		//T* obj = (T*)((T*)_last->_memory + _countin);
		_countin++;
		return new(obj)T;

	}

	void  Delete(T* ptr)
	{
		//先调用析构函数，然后把要析构的对象的内存块返回到内存池当中，也就是头插入自由链表_lastdelete中。
		
		if (ptr)
		{
			ptr->~T();
			*(T**)ptr = _lastDelete;
			_lastDelete = ptr;
		}
		
		
	}
private:
	size_t _countin;	//记录当前节点使用的计数
	BlockNode* _first;
	BlockNode* _last;
	size_t _Maximum;	//记录最大节点最大的对象数目
	static size_t _itemsize ;	//单个对象的大小
	T* _lastDelete;			//维护内存池

};
template<class T>
 size_t objectpool<T>::IninItemSize()
{
	//BlockNode中存储了void* 的一个指针，所以最低限度你要开出来一个能存放void*的指针的大小的对象空间。
	if (sizeof(T) <= sizeof(void*))
		return sizeof(void*);
	else
		return sizeof(T);
}

template<class T>
 size_t objectpool<T>::_itemsize = objectpool<T>::IninItemSize();
