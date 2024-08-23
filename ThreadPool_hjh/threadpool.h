// �����linux�¾Ͳ�һ��������#pragma once

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

// Any���ͣ����Խ����������ݵ�����
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
		// linux��condition_variable����������ʲôҲ����
		// ��������״̬ʧЧ���޹�����
		cond_.notify_all();
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

class Task;

// ʵ�ֽ����ύ���̳߳ص�task����ִ����ɺ�ķ���ֵ����Result  �û�ɶʱ���ý�����߳�ɶʱ����������������������������
class Result {
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;

	// ����һ��setVal��������ȡ����ִ����ķ���ֵ��
	void setVal(Any any);
	// �������get�������û���������Ŵ��ȡtask�ķ���ֵ
	Any get();
private:
	Any any_;
	Semaphore sem_;
	std::shared_ptr<Task> task_;
	std::atomic_bool isValid_;
};

// ����������
class Task {
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* res);

	// �û������Զ��������������ͣ���Task�̳У���дrun������ʵ���Զ���������
	virtual Any run() = 0;
private:
	// result��������Ҫ����task�����ﲻ��������ָ�룬��Ϊѭ�������ˣ�����
	Result* result_;
};

// �߳�֧��ģʽ enum��ֱ��ʹ��ö�����������enum����һ�����У����Լ�class
enum class PoolMode {
	MODE_FIXED, // �̶������߳�
	MODE_CACHED // �߳������ɶ�̬����
};

// �߳�����
class Thread {
public:
	// �̺߳�����������
	using ThreadFunc = std::function<void(int)>;

	// �̹߳���
	Thread(ThreadFunc func);

	// �߳�����
	~Thread();

	// �����߳�
	void start();

	// ��ȡ�߳�id
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
		void run(){//�̴߳���};
	}
	pool.submitTask(std::make_shared<MyTask>());
	makeshared���԰Ѷ�������ü���һ�𴴽�����ֹ�ڴ�����ˣ����޷��ͷţ�
	*/

	//�̳߳�����
class ThreadPool {
public:
	ThreadPool(); // �й����������

	~ThreadPool();

	// �����̳߳ع���ģʽ
	void setMode(PoolMode mode);

	// ����task�������������ֵ
	void setTaskQueMaxThreshHold(int threshhold);

	// ����cachedģʽ���߳���ֵ
	void setThreadMaxThreshHold(int threshhold);

	// ���̳߳��ύ����
	Result submitTask(std::shared_ptr<Task> sp);

	// �����̳߳�
	void start(int initThreadSize = std::thread::hardware_concurrency());

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator = (const ThreadPool&) = delete;

private:
	// �����̺߳���
	void threadFunc(int threadid);

	// ���pool������״̬
	bool checkRunningState() const;
private:
	// ָ��û���������������Կ���������ָ��
	//std::vector<std::unique_ptr<Thread>> threads_; // �߳��б�
	std::unordered_map<int, std::shared_ptr<Thread>> threads_;

	size_t initThreadSize_; // ��ʼ�߳�����
	// ��¼��ǰ�̳߳������̵߳�������
	std::atomic_int curThreadSize_;
	int threadSizeThreshHold_; // �߳�����������ֵ
	std::atomic_int idleThreadSize_;

	// ��Ҫ��ָ����������ڳ�����run֮�󣬲�����ָ��
	std::queue<std::shared_ptr<Task>> taskQue; // �������
	std::atomic_int taskSize_; // ���������
	int taskQueMaxThreshHold_; // ������е�������ֵ

	std::mutex taskQueMtx_; // ��֤������е��̰߳�ȫ
	std::condition_variable notFull_; // ��ʾ������в���
	std::condition_variable notEmpty_; // ��ʾ������в���
	std::condition_variable exitCond_; // ��ʾ�û����̽���


	PoolMode poolMode_; // ��ǰ�̳߳ع���ģʽ
	// ����̶߳����У������漰�̰߳�ȫ
	std::atomic_bool isPoolRunning_;

};
#endif // !THREADPOOL_H
