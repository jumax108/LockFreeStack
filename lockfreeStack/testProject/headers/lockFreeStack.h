#pragma once

#include "dump/headers/dump.h"
#include "objectFreeList/headers/objectFreeListTLS.h"

#pragma comment(lib, "lib/dump/dump")
#pragma comment(lib, "lib/objectFreeList/ObjectFreeListTLS")

static constexpr unsigned __int64 reUseCntMask = 0xFFFFF80000000000;
static constexpr unsigned __int64 pointerMask  = 0x000007FFFFFFFFFF;
template <typename T>
class CLockFreeStack{

public:

	CLockFreeStack();
	~CLockFreeStack();

	bool push(T data, HANDLE thread = NULL);
	bool pop(T* data, HANDLE thread = NULL);

	unsigned int getSize();

private:

	struct stNode{
		
		stNode(){
			_data = NULL;
			_next = nullptr;
		}

		T _data;
		void* _next;
	};

	void* _top;
		
	// ���� �޸𸮸� �˻��ϱ� ���� ����
	// push�ϸ� ��� ������Ű�� 21�� ��Ʈ�� ptr ���� �־ ����
	// 0x0000080000000000 �׻� �� ���� ������Ų��. (1<<43)
	// ���� 1�� ������Ű�� �����Ͷ� ��ĥ �� ��� ����Ʈ �ؾ���
	unsigned __int64 _nodeChangeCnt;

	unsigned short _capacity;

	unsigned int _size;

	CObjectFreeListTLS<stNode> _nodeFreeList;

};
 
template<typename T>
CLockFreeStack<T>::CLockFreeStack():
	_nodeFreeList(false, false){

	_size = 0;
	_top = 0;

	_pushCnt = 0;

}

template<typename T>
CLockFreeStack<T>::~CLockFreeStack(){
}

template <typename T>
bool CLockFreeStack<T>::push(T data, HANDLE thread){
	
	stNode* newNode = _nodeFreeList->allocObject();
	newNode->_data = data;
	
	void* newNodePtr;
	void* top;
	stNode* topNode;
	
	InterlockedAdd64((LONG64*)&_nodeChangeCnt, 0x0000080000000000);

	do {
		
		top = _top;

		newNode->_next = top;
		newNodePtr = (void*)((_nodeChangeCnt) | (unsigned __int64)newNode);
		
	} while(InterlockedCompareExchange64((__int64*)&_top, (__int64)newNodePtr, (__int64)top) != (__int64)top);

	return true;

}

template<typename T>
bool CLockFreeStack<T>::pop(T* data, HANDLE thread){

	stNode* topNode;
	void* topNextPtr;
	void* top;
	
	do {

		top = _top;
		
		topNode = (stNode*)((unsigned __int64)top & pointerMask);

	} while(InterlockedCompareExchange64((__int64*)&_top, (__int64)topNode->_next, (__int64)top) != (__int64)top);
	
	*data = topNode->_data;

	_nodeFreeList->freeObject(topNode);

	return true;
}

template <typename T>
unsigned int CLockFreeStack<T>::getSize(){
	return _size;
}