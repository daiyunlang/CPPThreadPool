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

	//工作线程执行函数
	void worker();

	//管理者线程执行函数
	void Manager();

	//添加任务
	template<typename F, typename ... Args>
	auto EnQueue(F && f, Args && ... args) -> std::future<typename std::result_of<F(Args ...)>::type> {
		//返回值类型的别名
		using tReturnType = typename std::result_of<F(Args ...)>::type;
		//将任务放入可调用对象包装器
		auto tTask = std::make_shared<std::packaged_task<tReturnType()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args) ...)
		);

		std::future<tReturnType> tRes = tTask->get_future();

		std::unique_lock<std::mutex> tUniLock(_poolMutex);
		
		//之前未使用共享指针，直接使用packaged_task，在lambda表达式中，若直接使用值传递将会导致使用const packaged_task
		//编译不过，若直接使用引用传递，tTask本身就是一个局部变量，后续调用将出现问题
		//使用共享指针好像能解决这问题
		_que.push([tTask]() {
			(*tTask)();
		});
		tUniLock.unlock();
		_poolCondVar.notify_one();

		return tRes;
	}

	int GetLiveThreadNum();

private:
	//任务队列
	std::queue<std::function<void()>> _que;//_poolMutex

	//工作线程队列
	std::map<std::thread::id, std::thread> _thread;//_threadQueMutex

	//管理者线程
	std::thread _managerThread;

	//线程池锁
	std::mutex _poolMutex;

	//工作线程队列锁
	std::mutex _threadQueMutex;

	//忙的线程个数锁
	std::mutex _busyThreadNumMutex;

	//线程池是否停止
	std::atomic<bool> _isStop;

	//条件变量
	std::condition_variable _poolCondVar;

	//最大线程数
	int _maxThreadNum;
	//最小线程数
	int _minThreadNum;
	//默认线程数
	int _defaultThreadNum;
	//忙的线程数
	int _busyThreadNum;//_busyThreadNumMutex
	//需要销毁的线程个数
	int _needExitThreadNum;//_poolMutex
};

