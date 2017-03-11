#define _CRT_SECURE_NO_WARNINGS 1
#pragma once
#include<iostream>
#include<cstdlib>

using namespace std;


template<typename T>
class objectpool
{
	//���ȸ����ڴ�صĽڵ㣬
	struct BlockNode
	{
		void* _memory;			//��Ӧÿ���ڵ����ҵ��ڴ��
		BlockNode * _next;		//ָ����һ���ڵ��ָ�롣
		size_t _objNum;			//��ʾ�ڴ����ĸ���
		//�����ڴ��ĳ�ʼ��
		BlockNode(size_t objnum)
			: _next(NULL)
			, _objNum(objnum)
			
		{
			_memory = malloc(_itemsize*objnum);
		}
		//�ڴ������
		~BlockNode()
		{
			free(_memory);
			_memory = NULL;
			_next = NULL;
			_objNum = 0;
		}
	};
public:
	//������ʼ�������С�ĺ���
	static size_t IninItemSize();
	//�̶���С����ش��캯��
	objectpool(size_t initNum = 32, size_t maxNum = 100000)
		:_countin(0)
		, _Maximum(maxNum)
	{
		_first = _last = new BlockNode(initNum);
		_lastDelete = NULL;
	}
	//����ص�����
	~objectpool()
	{
		Destory();
		_first = _last = NULL;



		////�������е��ڴ������ͷ��Ժ����ʱ��_lastdelete�����������ڴ��
		//
		//if (_lastDelete==NULL)
		//	cout << "�������ڴ�й©" << endl;

	}
	void Destory()
	{
		//���������ڴ�ص�ʱ���������ʱ���ȡ�ķ�ʽ�����ͷ�ÿһ���ڵ�ҵ�memory��
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
		//�����ͷ���ǰ���ڴ��ַ�ռ�
		//�ڴ�������������
		//�����µĽڵ����
		if (_lastDelete)
		{
			T* obj = _lastDelete;
			 //ǿתΪT**��Ȼ������á���������32λ�����64Ϊ��������ƣ�T**ֱ��ȡ������ȷ�ĵ�ַ���ݡ�
			_lastDelete = *((T**)_lastDelete);

			return new(obj)T;
		}


		//��BlockNode���˵�ʱ�����ʱ�����Ҫȥ����һ��BlockNodeȥ�������
		if (_countin == _last->_objNum)
		{
			size_t newNodeNum = _last->_objNum * 2;			
			if (newNodeNum >= _Maximum)
				newNodeNum = _Maximum;
			_last->_next = new BlockNode(newNodeNum);
			if (_last->_next == NULL)
				cout << "�ڴ濪��ʧ��" << endl;
			_last = _last->_next; 
			_countin = 0;
		}

		//��memory��ʣ���λ�ý��й���
		T* obj = (T*)((char*)_last->_memory + _countin*_itemsize);
		//T* obj = (T*)((T*)_last->_memory + _countin);
		_countin++;
		return new(obj)T;

	}

	void  Delete(T* ptr)
	{
		//�ȵ�������������Ȼ���Ҫ�����Ķ�����ڴ�鷵�ص��ڴ�ص��У�Ҳ����ͷ������������_lastdelete�С�
		
		if (ptr)
		{
			ptr->~T();
			*(T**)ptr = _lastDelete;
			_lastDelete = ptr;
		}
		
		
	}
private:
	size_t _countin;	//��¼��ǰ�ڵ�ʹ�õļ���
	BlockNode* _first;
	BlockNode* _last;
	size_t _Maximum;	//��¼���ڵ����Ķ�����Ŀ
	static size_t _itemsize ;	//��������Ĵ�С
	T* _lastDelete;			//ά���ڴ��

};
template<class T>
 size_t objectpool<T>::IninItemSize()
{
	//BlockNode�д洢��void* ��һ��ָ�룬��������޶���Ҫ������һ���ܴ��void*��ָ��Ĵ�С�Ķ���ռ䡣
	if (sizeof(T) <= sizeof(void*))
		return sizeof(void*);
	else
		return sizeof(T);
}

template<class T>
 size_t objectpool<T>::_itemsize = objectpool<T>::IninItemSize();
