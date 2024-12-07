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
	//��ʼ�������߳�
	for (int i = 0; i < _defaultThreadNum; ++i) {
		//��Ҫע��˴��̵߳ĵ������Ա������������qt��qtconcurrent::run��ͬ��this�ڵڶ�������λ�ã�����
		std::thread tThread(&ThreadPool::worker, this);
		//std::thread tThread([this]() {
		//	this->worker();
		//});
		//��¼�������̣߳��߳�id���̱߳���
		_thread.insert(make_pair(tThread.get_id(), std::move(tThread)));
	}

	//�����������߳�
	_managerThread = std::move(std::thread( &ThreadPool::Manager, this));
}

ThreadPool::~ThreadPool()
{
	_isStop = true;

	_managerThread.join();

	_poolCondVar.notify_all();

	std::unique_lock<std::mutex> tUniLock(_threadQueMutex);
	//����
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
		//��ȡ��
		std::unique_lock<std::mutex> tUniLock(_poolMutex);
		//����������ֱ���̳߳�ֹͣ������������зǿ�
		_poolCondVar.wait(tUniLock, [this]() {
			return _isStop || !_que.empty();
		});

		//�����߳�
		if (_needExitThreadNum > 0) {
			--_needExitThreadNum;
			if (GetLiveThreadNum() > _minThreadNum) {
				tUniLock.unlock();
				return;
			}
		}

		//���̳߳�ֹͣ�����������Ϊ�գ����˳������߳�
		if (_isStop && _que.empty()) {
			tUniLock.unlock();
			return;
		}

		//ȡ������
		std::function<void()> tTask = std::move(_que.front());

		_que.pop();
		tUniLock.unlock();

		std::unique_lock<std::mutex> tCBusyLock(_busyThreadNumMutex);
		++_busyThreadNum;
		tCBusyLock.unlock();

		//ִ��
		tTask();

		std::unique_lock<std::mutex> tDBusyLock(_busyThreadNumMutex);
		--_busyThreadNum;
		tDBusyLock.unlock();
	}
}

void ThreadPool::Manager()
{
	while (!_isStop) {
		//ÿ������һ��
		std::this_thread::sleep_for(std::chrono::seconds(5));

		//��ȡ��ǰ������и���
		std::unique_lock<std::mutex> tPoolUniLock(_poolMutex);
		int tWaitingTaskNum = _que.size();
		tPoolUniLock.unlock();

		//��ȡ��ǰæ���߳���
		std::unique_lock<std::mutex> tBusyLock(_busyThreadNumMutex);
		int tBusyThreadNum = _busyThreadNum;
		tBusyLock.unlock();

		std::unique_lock<std::mutex> tUniLock(_threadQueMutex);
		int tCurLiveThreadNum = _thread.size();
		//����̣߳�ÿ�����һ��
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

		//�����߳�
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
	//����Ѿ��������߳�
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
