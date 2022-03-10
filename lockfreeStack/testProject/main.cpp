#include <stdio.h>
#include <thread>

#include "../headers/lockFreeStack.h"

constexpr int THREAD_NUM = 5;
constexpr int NODE_NUM_EACH_THREAD = 100;

constexpr int TOTAL_NODE_NUM = THREAD_NUM * NODE_NUM_EACH_THREAD;

CDump dump;

struct stNode{

	stNode(){
		num = 0;
	}

	int num;

};

CLockFreeStack<stNode*>* lockFreeStack = new CLockFreeStack<stNode*>();
CLockFreeStack<stNode*>* stackForDebug = nullptr;

unsigned __stdcall logicTestFunc(void* args);

int main(){


	for(int nodeCnt = 0; nodeCnt < TOTAL_NODE_NUM; ++nodeCnt){
		lockFreeStack->push(new stNode);
	}

	for(int threadCnt = 0; threadCnt < THREAD_NUM; ++threadCnt){
		_beginthreadex(nullptr, 0, logicTestFunc, nullptr, 0, nullptr);
	}

	for(;;){
		printf("stack Size: %d\n", lockFreeStack->getSize());
		Sleep(999);
	}

	return 0;

}

unsigned __stdcall logicTestFunc(void* args){

	stNode* nodes[NODE_NUM_EACH_THREAD];

	for(;;){
		
		///////////////////////////////////////////////////
		// 1. stack에서 node를 pop
		for(int nodeCnt = 0; nodeCnt < NODE_NUM_EACH_THREAD; ++nodeCnt){
			lockFreeStack->pop(&nodes[nodeCnt]);
		}
		///////////////////////////////////////////////////
		
		///////////////////////////////////////////////////
		// 2. node의 데이터가 0인지 확인
		for(int nodeCnt = 0; nodeCnt < NODE_NUM_EACH_THREAD; ++nodeCnt){
			if(nodes[nodeCnt]->num != 0){
				stackForDebug = lockFreeStack;
				lockFreeStack = nullptr;
				CDump::crash();
			}
		}
		///////////////////////////////////////////////////

		///////////////////////////////////////////////////
		// 3. 데이터 1 증가
		for(int nodeCnt = 0; nodeCnt < NODE_NUM_EACH_THREAD; ++nodeCnt){
			InterlockedIncrement((LONG*)&nodes[nodeCnt]->num);
		}
		///////////////////////////////////////////////////

		///////////////////////////////////////////////////
		// 4. node의 데이터가 1인지 확인
		for(int nodeCnt = 0; nodeCnt < NODE_NUM_EACH_THREAD; ++nodeCnt){
			if(nodes[nodeCnt]->num != 1){
				stackForDebug = lockFreeStack;
				lockFreeStack = nullptr;
				CDump::crash();
			}
		}
		///////////////////////////////////////////////////

		///////////////////////////////////////////////////
		// 5. 데이터 1 감소
		for(int nodeCnt = 0; nodeCnt < NODE_NUM_EACH_THREAD; ++nodeCnt){
			InterlockedDecrement((LONG*)&nodes[nodeCnt]->num);
		}
		///////////////////////////////////////////////////

		///////////////////////////////////////////////////
		// 6. node의 데이터가 0인지 확인
		for(int nodeCnt = 0; nodeCnt < NODE_NUM_EACH_THREAD; ++nodeCnt){
			if(nodes[nodeCnt]->num != 0){
				stackForDebug = lockFreeStack;
				lockFreeStack = nullptr;
				CDump::crash();
			}
		}
		///////////////////////////////////////////////////

		///////////////////////////////////////////////////
		// 7. stack에 삽입
		for(int nodeCnt = 0; nodeCnt < NODE_NUM_EACH_THREAD; ++nodeCnt){
			lockFreeStack->push(nodes[nodeCnt]);
		}
		///////////////////////////////////////////////////
	}

	return 0;
}