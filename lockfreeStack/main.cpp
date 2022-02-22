#include <Windows.h>
#include <stdio.h>
#include <thread>
#include <list>
#include <Dbghelp.h>
#include <conio.h>

#include "UnsafeObjectFreeList.h"
#include "lockFreeStack.h"
#include "dump.h"

int size = 0;

struct stNode{

	int data = 0;
	bool use;
};
HANDLE _printTpsThread;

CLockFreeStack<stNode*> stack;

CDump dump;

unsigned int tps = 0;

constexpr int THREAD_NUM = 3;
constexpr int NODE_EACH_THREAD = 1000;
constexpr int MAX_NODE = THREAD_NUM * NODE_EACH_THREAD;

stNode _numArr[MAX_NODE];

HANDLE _logicThread[THREAD_NUM];

HANDLE _threadExitEvent;

unsigned __stdcall logicTestFunc(void*);
unsigned __stdcall printTPS(void*);

int main(){

	_threadExitEvent = CreateEventW(nullptr, true, false, L"threadExit");

	_printTpsThread = (HANDLE)_beginthreadex(nullptr, 0, printTPS, nullptr, 0, nullptr);

	for(int threadCnt = 0; threadCnt < THREAD_NUM; ++threadCnt){

		_logicThread[threadCnt] = (HANDLE)_beginthreadex(nullptr, 0, logicTestFunc, &_logicThread[threadCnt], 0, nullptr);

	}

	while(_getwch() != L'q');

	SetEvent(_threadExitEvent);

	HANDLE* allThread = new HANDLE[THREAD_NUM + 1];
	
	for(int threadCnt = 0; threadCnt < THREAD_NUM ; ++threadCnt){

		allThread[threadCnt] = _logicThread[threadCnt];

	}
	allThread[THREAD_NUM] = _printTpsThread;

	WaitForMultipleObjects(THREAD_NUM + 1, allThread, true, INFINITE);

	delete[] allThread;

	return 0;

}

unsigned __stdcall logicTestFunc(void* arg){

	///
	/// 
	/// 테스트 시나리오
	/// 
	/// 1. nodeNum 만큼의 노드를 스택에 push 한다.
	/// 2. 스택에서 nodeNum 만큼의 노드를 pop 한다.
	/// 3. 노드의 데이터가 초기 데이터인지 확인한다.					-> 다른 스레드에서 사용했는지 확인
	/// 4. 노드의 데이터를 1 증가한다.
	/// 5. 노드의 데이터가 1 인지 확인한다.								-> 다른 스레드에서 사용했는지 확인
	/// 6. 노드의 데이터를 1 감소한다.
	/// 7. 노드의 데이터가 0 인지 확인한다.								-> 다른 스레드에서 사용했는지 확인
	/// 8. 반복한다.
	/// 

	HANDLE threadHandle = *(HANDLE*)arg;
	int nodeNum = NODE_EACH_THREAD;
	stNode** arr = new stNode*[nodeNum];
	
	for(;;){

		unsigned int threadExit = WaitForSingleObject(_threadExitEvent, 0);
		if(threadExit == WAIT_OBJECT_0){
			break;
		}

		int indexCnt = 0;
		for(int nodeCnt = 0; nodeCnt < nodeNum; ++indexCnt){

			bool used = InterlockedExchange8((char*)&_numArr[indexCnt].use, true);
			if(used == true){
				continue;
			}

			nodeCnt += 1;
			stNode* node = &_numArr[indexCnt];

			bool pushResult = stack.push(node, threadHandle);
			
			if(pushResult == false){
				// error
				CDump::crash();
			}
		}

		for(int nodeCnt = 0; nodeCnt < nodeNum; ++nodeCnt){

			bool popResult = stack.pop(&arr[nodeCnt], threadHandle);
						
			stNode* node = arr[nodeCnt];

			if(popResult == false){
				// error
				CDump::crash();
			}
		}

		for(int nodeCnt = 0; nodeCnt < nodeNum; ++nodeCnt){
			if(arr[nodeCnt]->data != 0){
				// error
				CDump::crash();
			}
		}

		for(int nodeCnt = 0; nodeCnt < nodeNum; ++nodeCnt){
		
			InterlockedIncrement((unsigned int*)&arr[nodeCnt]->data);
			
			stNode* node = arr[nodeCnt];
			
		}

		
		for(int nodeCnt = 0; nodeCnt < nodeNum; ++nodeCnt){
			if(arr[nodeCnt]->data != 1){
				// error
				CDump::crash();
			}
		}

		for(int nodeCnt = 0; nodeCnt < nodeNum; ++nodeCnt){
		
			stNode* node = arr[nodeCnt];
			InterlockedDecrement((unsigned int*)&node->data);
			
			
		}
		
		for(int nodeCnt = 0; nodeCnt < nodeNum; ++nodeCnt){
			if(arr[nodeCnt]->data  != 0){
				// error
				CDump::crash();
			}
			arr[nodeCnt]->use = false;
		}

		InterlockedIncrement(&tps);

	}

	delete[] (arr);

	return 0;
}

unsigned __stdcall printTPS(void*){

	while(1){
	
		unsigned int threadExit = WaitForSingleObject(_threadExitEvent, 0);
		if(threadExit == WAIT_OBJECT_0){
			break;
		}
		
		wprintf(L"TPS: %d\n", tps);
		InterlockedExchange(&tps, 0);
		Sleep(999);
	}

	return 0;

}