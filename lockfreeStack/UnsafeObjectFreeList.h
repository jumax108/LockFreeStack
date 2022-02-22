#ifndef __UNSAFE_OBJECT_FREE_LIST__

#define __UNSAFE_OBJECT_FREE_LIST__

template<typename T>
class CUnsafeObjectFreeList{

public:

	CUnsafeObjectFreeList(HANDLE heap, int _capacity = 0);
	~CUnsafeObjectFreeList();

	T* allocObject(HANDLE thread = nullptr);

	int freeObject(T* data, HANDLE thread = nullptr);

	inline unsigned int getCapacity() { return _capacity; }
	inline unsigned int getUsedCount() { return _usedCnt; }

private:

	struct stNode {

		stNode() {
			_nextNode = nullptr;
		}

		// alloc 함수에서 리턴할 실제 데이터
		T _data;

		// alloc 함수에서 할당 빠르게 하기 위함
		stNode* _nextNode;

	};

	struct stLogLine{

		char _code;
		unsigned __int64 _logID;
		HANDLE _thread;

		void* _nowTopNode;
		void* _nextTopNode;
		void* _pushNode;    // 새로 들어오는 노드
		void* _popNode;     // 이제 나갈 노드

	};

	stLogLine _log[65536];
	unsigned __int64 _logID;
	void* const NO_LOG_DATA = (void*)0xEEEEEEEEEEEEEEEE;

	// 메모리 할당, 해제를 위한 힙
	HANDLE _heap;

	// 사용 가능한 노드를 리스트의 형태로 저장합니다.
	// 할당하면 제거합니다.
	stNode* _freeNode;

	// 전체 노드 개수
	unsigned int _capacity;

	// 현재 할당된 노드 개수
	unsigned int _usedCnt;

	// alloc 횟수, 
	unsigned __int64 _allocCnt;

	unsigned __int64 _ptrMask = 0x000007FFFFFFFFFF;

	// 할당 받은 노드 전체의 리스트입니다.
	// 소멸자에서 전부 다 해제합니다.
	stNode* _allPtrList;

};
template <typename T>
CUnsafeObjectFreeList<T>::CUnsafeObjectFreeList(HANDLE heap, int size) {

	_freeNode = nullptr;

	_capacity = size;
	_usedCnt = 0;

	_logID = 0;

	_heap = heap;

	if (size == 0) {
		return;
	}

	for(int nodeCnt = 0; nodeCnt < size; ++nodeCnt){
		stNode* node = new stNode;
		node->_nextNode = _freeNode;
		_freeNode = node;
	}

}

template <typename T>
CUnsafeObjectFreeList<T>::~CUnsafeObjectFreeList() {

	while (_allPtrList != nullptr) {
		stNode* nextNode = _allPtrList->_nextNode;
		delete(_allPtrList);
		_allPtrList = nextNode;
	}

}

template<typename T>
T* CUnsafeObjectFreeList<T>::allocObject(HANDLE thread) {

    /*
	*  log code
	*  십의 자리 
	*      1, 대분류 alloc
	*  일의 자리
	*      1 : alloc 시작
	*      3 : InterlockedCompare 직전
	*      4 : InterlockedCompareExchange 완료 후, while 탈출
	*      9 : 남아 있는 노드가 없어서 new를 통한 추가 할당
    */

	stNode* allocNode = nullptr;
	stNode* nextNode = nullptr;
	stNode* freeNode = nullptr;
	void* nextPtr = nullptr;
	void* freePtr = nullptr;

	{
		// 1 : alloc 시작
		unsigned __int64 logID = InterlockedIncrement64((LONG64*)&_logID);
		unsigned short logIndex = (unsigned short)logID;
		stLogLine* logLine = &_log[logIndex];
		logLine->_code				= 0x11;
		logLine->_logID				= logID;
		logLine->_thread			= thread;
		logLine->_nowTopNode		= NO_LOG_DATA;
		logLine->_nextTopNode		= NO_LOG_DATA;
		logLine->_pushNode			= NO_LOG_DATA;
		logLine->_popNode			= NO_LOG_DATA;
	}

	do {
		
		freePtr = _freeNode;
		freeNode = (stNode*)((unsigned __int64)freePtr & _ptrMask);

		if (freeNode == nullptr) {

			allocNode = new stNode;
			InterlockedIncrement(&_capacity);

			{
				// 9 : 남아 있는 노드가 없어서 new를 통한 추가 할당
				unsigned __int64 logID = InterlockedIncrement64((LONG64*)&_logID);
				unsigned short logIndex = (unsigned short)logID;
				stLogLine* logLine = &_log[logIndex];
				logLine->_code				= 0x19;
				logLine->_logID				= logID;
				logLine->_thread			= thread;
				logLine->_nowTopNode		= freeNode;
				logLine->_nextTopNode		= freeNode;
				logLine->_pushNode			= NO_LOG_DATA;
				logLine->_popNode			= allocNode;
			}

			break;

		} 
	
		allocNode = freeNode;

		nextNode = freeNode->_nextNode;
		nextPtr = (void*)((unsigned __int64)nextNode | (_allocCnt << 43));
		
		{
			// 3 : InterlockedCompare 직전
			unsigned __int64 logID = InterlockedIncrement64((LONG64*)&_logID);
			unsigned short logIndex = (unsigned short)logID;
			stLogLine* logLine = &_log[logIndex];
			logLine->_code				= 0x13;
			logLine->_logID				= logID;
			logLine->_thread			= thread;			
			logLine->_nowTopNode		= freeNode;
			logLine->_nextTopNode		= nextNode;
			logLine->_pushNode			= NO_LOG_DATA;
			logLine->_popNode			= allocNode;
		}

	} while ( InterlockedCompareExchange64( (LONG64*)&_freeNode, (LONG64)nextPtr, (LONG64)freePtr ) != (LONG64)freePtr );
	
	{
		// 4 : InterlockedCompareExchange 완료 후, while 탈출
		unsigned __int64 logID = InterlockedIncrement64((LONG64*)&_logID);
		unsigned short logIndex = (unsigned short)logID;
		stLogLine* logLine = &_log[logIndex];
		logLine->_code				= 0x14;
		logLine->_logID				= logID;
		logLine->_thread			= thread;		
		logLine->_nowTopNode		= freeNode;
		logLine->_nextTopNode		= nextNode;
		logLine->_pushNode			= NO_LOG_DATA;
		logLine->_popNode			= allocNode;
	}


	InterlockedIncrement(&_usedCnt);
	InterlockedIncrement64((LONG64*)&_allocCnt);

	return &(allocNode->_data);
}

template <typename T>
int CUnsafeObjectFreeList<T>::freeObject(T* data, HANDLE thread) {


	 /*
	 *  log code
	 *  십의 자리 
	 *      2, 대분류 free
	 *  일의 자리
	 *      1 : free 시작
	 *      3 : InterlockedCompare 직전
	 *      4 : InterlockedCompareExchange 완료 후, while 탈출
     */

	stNode* usedNode = (stNode*)data;
	stNode* freeNode = nullptr;
	void* usedPtr;
	void* freePtr;
	
	{
		// 1 : free 시작
		unsigned __int64 logID = InterlockedIncrement64((LONG64*)&_logID);
		unsigned short logIndex = (unsigned short)logID;
		stLogLine* logLine = &_log[logIndex];
		logLine->_code				= 0x21;
		logLine->_logID				= logID;
		logLine->_thread			= thread;
		logLine->_nowTopNode		= NO_LOG_DATA;
		logLine->_nextTopNode		= usedNode;
		logLine->_pushNode			= usedNode;
		logLine->_popNode			= NO_LOG_DATA;
	}

	do {
	
		freePtr = _freeNode;
		freeNode = (stNode*)((unsigned __int64)freePtr & _ptrMask);

		usedNode->_nextNode = freeNode;

		usedPtr = (void*)((unsigned __int64)usedNode | (_allocCnt << 43));

		{
			// 3 : InterlockedCompare 직전
			unsigned __int64 logID = InterlockedIncrement64((LONG64*)&_logID);
			unsigned short logIndex = (unsigned short)logID;
			stLogLine* logLine = &_log[logIndex];
			logLine->_code				= 0x23;
			logLine->_logID				= logID;
			logLine->_thread			= thread;
			logLine->_nowTopNode		= freeNode;
			logLine->_nextTopNode		= usedNode;
			logLine->_pushNode			= usedNode;
			logLine->_popNode			= NO_LOG_DATA;
		}
	
	} while( InterlockedCompareExchange64( (LONG64*)&_freeNode, (LONG64)usedPtr, (LONG64)freePtr ) != (LONG64)freePtr );

	{
		// 4 : InterlockedCompareExchange 완료 후, while 탈출
		unsigned __int64 logID = InterlockedIncrement64((LONG64*)&_logID);
		unsigned short logIndex = (unsigned short)logID;
		stLogLine* logLine = &_log[logIndex];
		logLine->_code				= 0x24;
		logLine->_logID				= logID;
		logLine->_thread			= thread;
		logLine->_nowTopNode		= freeNode;
		logLine->_nextTopNode		= usedNode;
		logLine->_pushNode			= usedNode;
		logLine->_popNode			= NO_LOG_DATA;
	}

	InterlockedDecrement(&_usedCnt);


	return 0;
}
#endif