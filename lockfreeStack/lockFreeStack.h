#pragma once

template <typename T>
class CLockFreeStack{

public:

	CLockFreeStack();
	~CLockFreeStack();

	bool push(T data, HANDLE thread = NULL);
	bool pop(T* data, HANDLE thread = NULL);

	unsigned int getSize();

private:

	struct stLogLine{
		
		unsigned __int64 logID;
		HANDLE threadID;

		char code;

		void* topNode;
		void* nextTopNode;
		void* pushNode; // 새로 들어오는 노드 
		void* popNode;  // 이제 사라지는 노드
		unsigned __int64 stackPushCnt;  // 스택 전체에서 push가 반복된 횟수
		unsigned __int64 stackPopCnt;   // 스택 전체에서  pop이 반복된 횟수
		T data;

	};

	struct stNode{
		
		stNode(){
			_data = NULL;
			_next = nullptr;
		}

		T _data;
		void* _next;
	};

	void* _top;

	// 재사용 횟수용 마스크
	unsigned __int64 _reUseCntMask; 
	// stNode 포인터용 마스크
	unsigned __int64 _pointerMask;  
	
	// 재사용 메모리를 검사하기 위한 변수
	// push하면 계속 증가시키고 21개 비트를 ptr 값에 넣어서 비교함
	unsigned __int64 _nodeChangeCnt;
	
	// 로그용 변수
	// push하면 계속 증가
	// 같은 값이면 진입 후, 함수가 끝나지 않은 것이 보장됨
	unsigned __int64 _pushCnt;
	// 로그용 변수
	// pop하면 계속 증가
	// 같은 값이면 진입 후, 함수가 끝나지 않은 것이 보장됨
	unsigned __int64 _popCnt;

	unsigned short _capacity;

	unsigned int _size;

	stLogLine _log[65536] = {0,};
	unsigned __int64 _logID = 0;
				void* NO_LOG_PTR	= (void*)0xEEEEEEEEEEEEEEEE;
	unsigned __int64  NO_LOG_DATA	=		 0xEEEEEEEEEEEEEEEE;

	HANDLE _heap;

	CUnsafeObjectFreeList<stNode>* _nodeFreeList;

};
 
template<typename T>
CLockFreeStack<T>::CLockFreeStack(){

	_size = 0;
	_top = 0;

	_pushCnt = 0;

	_reUseCntMask = 0xFFFFF80000000000;
	_pointerMask  = 0x000007FFFFFFFFFF;

	_heap = HeapCreate(0, 0, 0);
	_nodeFreeList = new CUnsafeObjectFreeList<stNode>(_heap, 0);

}

template<typename T>
CLockFreeStack<T>::~CLockFreeStack(){
	delete _nodeFreeList;
}

template <typename T>
bool CLockFreeStack<T>::push(T data, HANDLE thread){

	/*
	* 
	*	log Code
	*	16^1의 자리
	*		1 : push
	*	16^0의 자리
	*		1 : 함수 진입 
	*		2 : InterlockedCompare 직전
	*		3 : 함수 리턴
	*/
	
	
	unsigned __int64 pushCnt = InterlockedIncrement(&_pushCnt);
	stNode* newNode = _nodeFreeList->allocObject(thread);
	newNode->_data = data;
	
	void* newNodePtr = nullptr;
	void* top = nullptr;
	stNode* topNode = nullptr;
	
	// 1 : 함수 진입
	{
		unsigned __int64 logID = InterlockedIncrement64((LONG64*)&_logID);
		unsigned short logIndex = (unsigned short)logID;
		stLogLine* logLine = &_log[logIndex];
		logLine->code			= 0x11;
		logLine->logID			= logID;
		logLine->threadID		= thread;
		logLine->topNode		= NO_LOG_PTR;
		logLine->nextTopNode	= NO_LOG_PTR;
		logLine->pushNode		= NO_LOG_PTR;
		logLine->popNode		= NO_LOG_PTR;
		logLine->stackPushCnt	= pushCnt;
		logLine->stackPopCnt	= NO_LOG_DATA;
		logLine->data			= data;
	}


	do {
		
		top = _top;

		topNode = (stNode*)((unsigned __int64)top & _pointerMask);
		newNode->_next = top;
		newNodePtr = (void*)((_nodeChangeCnt << 43) | (unsigned __int64)newNode);
		
		// 2 : InterlockedCompare 직전
		{
			unsigned __int64 logID = InterlockedIncrement64((LONG64*)&_logID);
			unsigned short logIndex = (unsigned short)logID;
			stLogLine* logLine = &_log[logIndex];
			logLine->code			= 0x12;
			logLine->logID			= logID;
			logLine->threadID		= thread;
			logLine->topNode		= topNode;
			logLine->nextTopNode	= newNode;
			logLine->pushNode		= newNode;
			logLine->popNode		= NO_LOG_PTR;
			logLine->stackPushCnt	= pushCnt;
			logLine->stackPopCnt	= NO_LOG_DATA;
			logLine->data			= data;
		}
		
	} while(InterlockedCompareExchange64((__int64*)&_top, (__int64)newNodePtr, (__int64)top) != (__int64)top);
	
	InterlockedIncrement64((LONG64*)&_nodeChangeCnt);
	// 3 : 함수 리턴
	{
		unsigned __int64 logID = InterlockedIncrement64((LONG64*)&_logID);
		unsigned short logIndex = (unsigned short)logID;
		stLogLine* logLine = &_log[logIndex];
		logLine->code			= 0x13;
		logLine->logID			= logID;
		logLine->threadID		= thread;
		logLine->topNode		= topNode;
		logLine->nextTopNode	= newNode;
		logLine->pushNode		= newNode;
		logLine->popNode		= NO_LOG_PTR;
		logLine->stackPushCnt	= pushCnt;
		logLine->stackPopCnt	= NO_LOG_DATA;
		logLine->data			= data;
	}


	return true;

}

template<typename T>
bool CLockFreeStack<T>::pop(T* data, HANDLE thread){

	/*
	* 
	*	log Code
	*	16^1의 자리
	*		2 : pop
	*	16^0의 자리
	*		1 : 함수 진입
	*		2 : InterlockedCompare 직전
	*		3 : InterlockedCompareExchange 완료
	*/
	
	unsigned __int64 popCnt = InterlockedIncrement64((LONG64*)&_popCnt);

	// 1 : 함수 진입
	{
		unsigned __int64 logID = InterlockedIncrement64((LONG64*)&_logID);
		unsigned short logIndex = (unsigned short)logID;
		stLogLine* logLine = &_log[logIndex];
		logLine->code			= 0x21;
		logLine->logID			= logID;
		logLine->threadID		= thread;
		logLine->topNode		= NO_LOG_PTR;
		logLine->nextTopNode	= NO_LOG_PTR;
		logLine->pushNode		= NO_LOG_PTR;
		logLine->popNode		= NO_LOG_PTR;
		logLine->stackPushCnt	= NO_LOG_DATA;
		logLine->stackPopCnt	= popCnt;
		logLine->data			= NULL;
	}

	stNode* topNode			= nullptr;
	stNode* topNextNode		= nullptr;
	void* topNextPtr		= nullptr;
	void* top				= nullptr;


	do {

		top = _top;
		popCnt = _popCnt;

		topNode = (stNode*)((unsigned __int64)top & _pointerMask);
		topNextPtr = topNode->_next;
		topNextNode = (stNode*)((unsigned __int64)topNextPtr & _pointerMask);
		
		// 2 : InterlockedCompare 직전
		{
			unsigned __int64 logID = InterlockedIncrement64((LONG64*)&_logID);
			unsigned short logIndex = (unsigned short)logID;
			stLogLine* logLine = &_log[logIndex];
			logLine->code			= 0x22;
			logLine->logID			= logID;
			logLine->threadID		= thread;
			logLine->topNode		= topNode;
			logLine->nextTopNode	= topNextNode;
			logLine->pushNode		= NO_LOG_PTR;
			logLine->popNode		= topNode;
			logLine->stackPushCnt	= NO_LOG_DATA;
			logLine->stackPopCnt	= popCnt;
			logLine->data			= topNode->_data;
		}
		

	} while(InterlockedCompareExchange64((__int64*)&_top, (__int64)topNextPtr, (__int64)top) != (__int64)top);
	
	InterlockedIncrement64((LONG64*)&_nodeChangeCnt);

	// 3 : InterlockedCompareExchange 완료
	{
		unsigned __int64 logID = InterlockedIncrement64((LONG64*)&_logID);
		unsigned short logIndex = (unsigned short)logID;
		stLogLine* logLine = &_log[logIndex];
		logLine->code			= 0x23;
		logLine->logID			= logID;
		logLine->threadID		= thread;
		logLine->topNode		= topNode;
		logLine->nextTopNode	= topNextNode;
		logLine->pushNode		= NO_LOG_PTR;
		logLine->popNode		= topNode;
		logLine->stackPushCnt	= NO_LOG_DATA;
		logLine->stackPopCnt	= popCnt;
		logLine->data			= topNode->_data;
	}
	

	*data = topNode->_data;

	_nodeFreeList->freeObject(topNode, thread);

	return true;
}

template <typename T>
unsigned int CLockFreeStack<T>::getSize(){
	return _size;
}