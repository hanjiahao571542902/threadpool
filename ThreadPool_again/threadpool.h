//#pragma once
#ifndef THREADPOOL_H
#define THREADPOOL_H
#include<memory>
#include<unordered_map>
#include<atomic>
#include<queue>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<iostream>
#include<thread>
#include<chrono>

class Any {
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	template<typename T>
	Any(T data) :base_(std::make_unique<Derived<T>>(data)) {};

	template<typename T>
	T Cast() {
		Derived<T>* pd = dynamic_cast<Derived<T>*>(base_.get());
		return pd->data_;
	}

public:
	class Base {
	public:
		virtual ~Base() = default;
	};
	template<typename T>
	class Derived :public Base {
	public:
		T data_;
		Derived(T data) :data_(data) {}
	};

	std::unique_ptr<Base> base_;
};

class Semaphore {
public:
	Semaphore(int resLimit = 0) :resLimit_(resLimit) {};
	~Semaphore() = default;
	void post() {
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}
	void wait() {
		std::unique_lock<std::mutex> lock(mtx_);
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

class Task;

class Result {
public:
	Result(std::shared_ptr<Task> task, bool isValid);
	~Result() = default;
	Any get();
	void setVal(Any any);
private:
	std::shared_ptr<Task> task_;
	Any any_;
	std::atomic_bool isValid_;
	Semaphore sem_;
};

class Task {
public:
	Task();
	~Task() = default;

	virtual Any run() = 0;

	void exec();
	void setResult(Result* res);
private:
	// 循环引用了！！！！！！！！！！！！！！！！！！！！！
	Result* res_;
};

class Thread {
public:
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc func);
	~Thread();
	int getId() const;
	void start();
private:
	int threadId_;
	static int generateId_;
	ThreadFunc func_;

};

enum class PoolMode {
	MODE_CACHED,
	MODE_FIXED
};
class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();
	void setMode(PoolMode mode = PoolMode::MODE_FIXED);
	void setTaskMaxThreshhold(int threshhold = 4);
	void setThreadMaxThreshhold(int threshhold = 4);
	Result submitTask(std::shared_ptr<Task> sp);
	void start(int inialThreadSize);
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	void threadFunc(int threadId);

private:
	PoolMode poolMode_;
	std::atomic_bool isPoolRunning_;

	// 线程队列
	int initThreadSize_;
	std::atomic_int curThreadSize_;
	std::atomic_int idleThreadSize_;
	int threadMaxThreshhold_;
	std::unordered_map<int, std::shared_ptr<Thread>> threads_;

	// 任务队列
	std::atomic_int taskSize_;
	int taskQueThreshHold_;
	std::queue<std::shared_ptr<Task>> taskQue;

	// 线程拿任务是一个要线程安全的生产者消费者
	std::mutex taskQueMtx_;
	std::condition_variable notFull_;
	std::condition_variable notEmpty_;

	std::condition_variable exitCond_;
};

#endif // THREADPOOL_H

