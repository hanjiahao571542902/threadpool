// 这个再linux下就不一定有用了#pragma once

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<Thread>
#include<iostream>
#include<unordered_map>

// Any类型：可以接收任意数据的类型
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	template<typename T>
	Any(T data) :base_(std::make_unique<Derive<T>>(data)) {};

	template<typename T>
	T cast_()
	{
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr) {
			throw "type is unmatch!";
		}
		return pd->data_;
	}


private:
	class Base
	{
	public:
		virtual ~Base() = default;


	};

	template<typename T>
	class Derive :public Base
	{
	public:
		T data_;
		Derive(T data) :data_(data) {}
	};

	std::unique_ptr<Base> base_;

};

class Semaphore {
public:
	Semaphore(int initSize = 0) :resLimit_(initSize) {};
	~Semaphore() = default;

	void wait() {
		std::unique_lock<std::mutex> lock(mtx_);
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}

	void post() {
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		// linux下condition_variable的析构函数什么也不做
		// 导致这里状态失效，无辜阻塞
		cond_.notify_all();
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

class Task;

// 实现接收提交到线程池的task任务执行完成后的返回值类型Result  用户啥时候拿结果和线程啥时候完成这个任务是在两个队列做的
class Result {
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;

	// 问题一：setVal方法，获取任务执行完的返回值的
	void setVal(Any any);
	// 问题二：get方法，用户调用这个放大获取task的返回值
	Any get();
private:
	Any any_;
	Semaphore sem_;
	std::shared_ptr<Task> task_;
	std::atomic_bool isValid_;
};

// 任务抽象基类
class Task {
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* res);

	// 用户可以自定义任意任务类型，从Task继承，重写run方法，实现自定义任务处理
	virtual Any run() = 0;
private:
	// result生命周期要大于task，这里不能用智能指针，因为循环引用了！！！
	Result* result_;
};

// 线程支持模式 enum是直接使用枚举项，所以两个enum，项一样不行，所以加class
enum class PoolMode {
	MODE_FIXED, // 固定数量线程
	MODE_CACHED // 线程数量可动态增长
};

// 线程类型
class Thread {
public:
	// 线程函数对象类型
	using ThreadFunc = std::function<void(int)>;

	// 线程构造
	Thread(ThreadFunc func);

	// 线程析构
	~Thread();

	// 启动线程
	void start();

	// 获取线程id
	int getId() const;
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;
};

/*
	example:
	ThreadPool pool;
	pool.start(4);

	class MyTask:public Task
	{
		public:
		void run(){//线程代码};
	}
	pool.submitTask(std::make_shared<MyTask>());
	makeshared可以把对象和引用计数一起创建，防止内存分配了，但无法释放，
	*/

	//线程池类型
class ThreadPool {
public:
	ThreadPool(); // 有构造就有析构

	~ThreadPool();

	// 设置线程池工作模式
	void setMode(PoolMode mode);

	// 设置task任务队列上限阈值
	void setTaskQueMaxThreshHold(int threshhold);

	// 设置cached模式下线程阈值
	void setThreadMaxThreshHold(int threshhold);

	// 给线程池提交任务
	Result submitTask(std::shared_ptr<Task> sp);

	// 开启线程池
	void start(int initThreadSize = std::thread::hardware_concurrency());

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator = (const ThreadPool&) = delete;

private:
	// 定义线程函数
	void threadFunc(int threadid);

	// 检查pool的运行状态
	bool checkRunningState() const;
private:
	// 指针没有析构函数，所以可以用智能指针
	//std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
	std::unordered_map<int, std::shared_ptr<Thread>> threads_;

	size_t initThreadSize_; // 初始线程数量
	// 记录当前线程池里面线程的总数量
	std::atomic_int curThreadSize_;
	int threadSizeThreshHold_; // 线程数量上限阈值
	std::atomic_int idleThreadSize_;

	// 你要让指针的生命周期持续到run之后，不能裸指针
	std::queue<std::shared_ptr<Task>> taskQue; // 任务队列
	std::atomic_int taskSize_; // 任务的数量
	int taskQueMaxThreshHold_; // 任务队列的上限阈值

	std::mutex taskQueMtx_; // 保证任务队列的线程安全
	std::condition_variable notFull_; // 表示任务队列不满
	std::condition_variable notEmpty_; // 表示任务队列不空
	std::condition_variable exitCond_; // 表示用户进程结束


	PoolMode poolMode_; // 当前线程池工作模式
	// 多个线程都会有，所以涉及线程安全
	std::atomic_bool isPoolRunning_;

};
#endif // !THREADPOOL_H
