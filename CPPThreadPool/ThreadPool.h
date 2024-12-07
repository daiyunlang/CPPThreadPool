#pragma once
#include <queue>
#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <map>
#include <future>

class ThreadPool
{
public:
	explicit ThreadPool(int tMaxThreadNum, int tMinThreadNum);
	~ThreadPool();

	//�����߳�ִ�к���
	void worker();

	//�������߳�ִ�к���
	void Manager();

	//�������
	template<typename F, typename ... Args>
	auto EnQueue(F && f, Args && ... args) -> std::future<typename std::result_of<F(Args ...)>::type> {
		//����ֵ���͵ı���
		using tReturnType = typename std::result_of<F(Args ...)>::type;
		//���������ɵ��ö����װ��
		auto tTask = std::make_shared<std::packaged_task<tReturnType()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args) ...)
		);

		std::future<tReturnType> tRes = tTask->get_future();

		std::unique_lock<std::mutex> tUniLock(_poolMutex);
		
		//֮ǰδʹ�ù���ָ�룬ֱ��ʹ��packaged_task����lambda���ʽ�У���ֱ��ʹ��ֵ���ݽ��ᵼ��ʹ��const packaged_task
		//���벻������ֱ��ʹ�����ô��ݣ�tTask�������һ���ֲ��������������ý���������
		//ʹ�ù���ָ������ܽ��������
		_que.push([tTask]() {
			(*tTask)();
		});
		tUniLock.unlock();
		_poolCondVar.notify_one();

		return tRes;
	}

	int GetLiveThreadNum();

private:
	//�������
	std::queue<std::function<void()>> _que;//_poolMutex

	//�����̶߳���
	std::map<std::thread::id, std::thread> _thread;//_threadQueMutex

	//�������߳�
	std::thread _managerThread;

	//�̳߳���
	std::mutex _poolMutex;

	//�����̶߳�����
	std::mutex _threadQueMutex;

	//æ���̸߳�����
	std::mutex _busyThreadNumMutex;

	//�̳߳��Ƿ�ֹͣ
	std::atomic<bool> _isStop;

	//��������
	std::condition_variable _poolCondVar;

	//����߳���
	int _maxThreadNum;
	//��С�߳���
	int _minThreadNum;
	//Ĭ���߳���
	int _defaultThreadNum;
	//æ���߳���
	int _busyThreadNum;//_busyThreadNumMutex
	//��Ҫ���ٵ��̸߳���
	int _needExitThreadNum;//_poolMutex
};

