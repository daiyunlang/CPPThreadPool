#include "ThreadPool.h"
#include <chrono>

const int DEFAULTTHREADNUM = 4;
const int CREATETHREADNUM = 1;
const int DESTROYTHREADNUM = 1;

ThreadPool::ThreadPool(int tMaxThreadNum, int tMinThreadNum)
	:_isStop(false)
	,_maxThreadNum(tMaxThreadNum)
	,_minThreadNum(tMinThreadNum)
	,_defaultThreadNum(DEFAULTTHREADNUM)
	, _busyThreadNum(0)
	, _needExitThreadNum(0)
{
	//初始化工作线程
	for (int i = 0; i < _defaultThreadNum; ++i) {
		//需要注意此处线程的调用类成员函数方法，与qt的qtconcurrent::run不同，this在第二个参数位置！！！
		std::thread tThread(&ThreadPool::worker, this);
		//std::thread tThread([this]() {
		//	this->worker();
		//});
		//记录工作的线程，线程id和线程本身
		_thread.insert(make_pair(tThread.get_id(), std::move(tThread)));
	}

	//启动管理者线程
	_managerThread = std::move(std::thread( &ThreadPool::Manager, this));
}

ThreadPool::~ThreadPool()
{
	_isStop = true;

	_managerThread.join();

	_poolCondVar.notify_all();

	std::unique_lock<std::mutex> tUniLock(_threadQueMutex);
	//分离
	std::map<std::thread::id, std::thread>::iterator iter = _thread.begin();
	for (; iter != _thread.end(); ) {
		std::thread::id tID = iter->first;
		if (iter->second.joinable()) {
			iter->second.join();
		}
		++iter;
	}
}

void ThreadPool::worker()
{
	while (true) {
		//获取锁
		std::unique_lock<std::mutex> tUniLock(_poolMutex);
		//条件变量，直到线程池停止，或者任务队列非空
		_poolCondVar.wait(tUniLock, [this]() {
			return _isStop || !_que.empty();
		});

		//销毁线程
		if (_needExitThreadNum > 0) {
			--_needExitThreadNum;
			if (GetLiveThreadNum() > _minThreadNum) {
				tUniLock.unlock();
				return;
			}
		}

		//若线程池停止，且任务队列为空，则退出工作线程
		if (_isStop && _que.empty()) {
			tUniLock.unlock();
			return;
		}

		//取出任务
		std::function<void()> tTask = std::move(_que.front());

		_que.pop();
		tUniLock.unlock();

		std::unique_lock<std::mutex> tCBusyLock(_busyThreadNumMutex);
		++_busyThreadNum;
		tCBusyLock.unlock();

		//执行
		tTask();

		std::unique_lock<std::mutex> tDBusyLock(_busyThreadNumMutex);
		--_busyThreadNum;
		tDBusyLock.unlock();
	}
}

void ThreadPool::Manager()
{
	while (!_isStop) {
		//每三秒检查一次
		std::this_thread::sleep_for(std::chrono::seconds(5));

		//获取当前任务队列个数
		std::unique_lock<std::mutex> tPoolUniLock(_poolMutex);
		int tWaitingTaskNum = _que.size();
		tPoolUniLock.unlock();

		//获取当前忙的线程数
		std::unique_lock<std::mutex> tBusyLock(_busyThreadNumMutex);
		int tBusyThreadNum = _busyThreadNum;
		tBusyLock.unlock();

		std::unique_lock<std::mutex> tUniLock(_threadQueMutex);
		int tCurLiveThreadNum = _thread.size();
		//添加线程，每次添加一个
		if (tWaitingTaskNum > tCurLiveThreadNum && tCurLiveThreadNum < _maxThreadNum) {
			for (int i = 0; i < CREATETHREADNUM; ++i) {
				std::thread tThread(&ThreadPool::worker, this);
				//std::thread tThread([this]() {
				//	this->worker();
				//});
				_thread.insert(make_pair(tThread.get_id(), std::move(tThread)));
			}
		}
		tUniLock.unlock();

		//销毁线程
		if (tBusyThreadNum * 2 < tCurLiveThreadNum && tCurLiveThreadNum > _minThreadNum) {
			std::unique_lock<std::mutex> tPoolUniLock(_poolMutex);
			_needExitThreadNum = DESTROYTHREADNUM;
			tPoolUniLock.unlock();

			for (int i = 0; i < DESTROYTHREADNUM; ++i) {
				_poolCondVar.notify_one();
			}
		}
	}
	printf("manager exit");
}

int ThreadPool::GetLiveThreadNum()
{
	std::unique_lock<std::mutex> tUniLock(_threadQueMutex);
	//清空已经结束的线程
	std::map<std::thread::id, std::thread>::iterator iter = _thread.begin();
	for (; iter != _thread.end(); ) {
		std::thread::id tID = iter->first;
		if (!iter->second.joinable()) {
			iter = _thread.erase(iter);
			continue;
		}
		++iter;
	}
	return _thread.size();
}
