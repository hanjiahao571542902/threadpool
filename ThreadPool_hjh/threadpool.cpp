#include "threadpool.h"


const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 6;
const int THREAD_MAX_IDLE_TIME = 10;// ��


ThreadPool::ThreadPool() // �й����������
	:initThreadSize_(0)
	, taskSize_(0)
	, idleThreadSize_(0)
	, curThreadSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)// ��ͬ�û�������ͬ
	, poolMode_(PoolMode::MODE_FIXED)
	, isPoolRunning_(false)
{}
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;



	// �ȴ��̳߳����������̷߳���  ������״̬������ & ����ִ������
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	// û�������̶߳���wait�������Ȱ�����������״̬
	notEmpty_.notify_all();
	std::cout << "Ҫ������" << std::endl;
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
	std::cout << "�������" << std::endl;
}

// �����̳߳ع���ģʽ
void ThreadPool::setMode(PoolMode mode) {
	if (checkRunningState()) {
		return;
	}
	poolMode_ = mode;
}

// ����task�������������ֵ
void ThreadPool::setTaskQueMaxThreshHold(int threshhold) {
	if (checkRunningState()) {
		return;
	}
	taskQueMaxThreshHold_ = threshhold;
}

// ����cachedģʽ���߳���ֵ
void ThreadPool::setThreadMaxThreshHold(int threshhold) {
	if (checkRunningState()) {
		return;
	}
	if (poolMode_ == PoolMode::MODE_CACHED) {
		threadSizeThreshHold_ = threshhold;
	}
}

// ���̳߳��ύ���� �û����øýӿڣ��������������������
Result ThreadPool::submitTask(std::shared_ptr<Task> sp) {
	// ��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	// ������״̬    wait  wait_for   wait_until
	// wait���ȣ����������Լ�ʱ�������wait_for�ȶ೤  waituntil�ȵ�������
	// �û��ύ�����������������1s�������ж��ύ����ʧ�ܣ�����
	// ���һ�뵽�˵ȵ��ˣ���ô�����أ�����ֵ�Ǹ�bool
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskQue.size() < (size_t)taskQueMaxThreshHold_; })) {
		// ����1s���ǲ���
		std::cerr << "task queue is full,submit task fail." << std::endl;
		return Result(sp, false);
		// return Result(task) ����task->getResult()???
		// �߳�ִ����task��task�ͱ������ˣ�task���������ӳ����õ�res
	}

	// �п�����ύ����
	taskQue.emplace(sp);
	taskSize_++;

	// �����������ˣ�����
	notEmpty_.notify_all();

	// cachedģʽ��������ȽϽ���  ������С��������� ��Ҫ���ݿ����߳������������������ж��Ƿ���Ҫ�������߳�
	// ��ʱ�������ʺ�cached����Ϊ�ᴴ���ܶ��߳�
	if (poolMode_ == PoolMode::MODE_CACHED
		&& idleThreadSize_ < taskSize_
		&& curThreadSize_ < threadSizeThreshHold_) {

		std::cout << ">>> create new thread" << std::endl;

		// �������߳�
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		threads_[threadId]->start();

		//�޸�����߳�����
		curThreadSize_++;
		idleThreadSize_++;
	}


	return Result(sp, true);
}

// �����̳߳�
void ThreadPool::start(int initThreadSize) {
	isPoolRunning_ = true;
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

	// �����̶߳���
	// �ȴ�����������������Ϊ�˹�ƽ,����������
	for (int i = 0; i < initThreadSize_; ++i) {
		// �����̶߳����ǣ����̺߳��������̶߳���
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
	}

	// ���������߳� std::vector<Thread*> threads_;
	for (int i = 0; i < initThreadSize_; i++) {
		threads_[i]->start();
		idleThreadSize_++; //��¼��ʼ�����߳�
	}
}

// �����̺߳��� �̳߳ص������̴߳����������������
void ThreadPool::threadFunc(int threadid) { // �̺߳�����������Ӧ���߳�Ҳ����
	//std::cout << "begin threadFunc tid: " << std::this_thread::get_id()<< std::endl;
	//std::cout << "end threadFunc  tid: " << std::this_thread::get_id() << std::endl;

	auto lastTime = std::chrono::high_resolution_clock().now();

	//	�����������ִ����ɣ��̳߳زſ��Ի��������߳���Դ
	// ����������ȥִ�����񣬲�ȻЧ��̫����
	while (1) {
		std::shared_ptr<Task> task;
		{
			// ��ȡ��
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid: " << std::this_thread::get_id() << "���Ի�ȡ���񡣡�" << std::endl;

			// cachedģʽ�£��п����Ѿ������ܶ��̣߳����ǿ���ʱ�䳬��60s��Ӧ�ý�������
			// ����initThreadSize_���߳�Ҫ���л���
			// ��ǰʱ��-��һ���߳�ִ�е�ʱ�� > 60s
				// ÿһ�뷵��һ�� ��ô���ֳ�ʱ���ػ����������ִ�з���
			// ��+˫���ж�
			while (taskQue.size() == 0) {
				// �̳߳�Ҫ�����������߳���Դ
				if (!isPoolRunning_)
				{
					// ����������̻߳�ͷ������while��false�������������ͷ���Դ
					threads_.erase(threadid);

					std::cout << "׼����������߳̿����̳߳ع�������threadid:" << std::this_thread::get_id() << " exit!" << std::endl;;
					exitCond_.notify_all();
					return;
				}
				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					if (std::cv_status::timeout ==
						notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						// ����������ʱ����
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_) {
							// ��ʼ���յ�ǰ�߳�
							// ��¼�߳���������ر�����ֵ���޸�
							// ���̶߳���Ӷ����б��������޸� û�취 threadfuncƥ���ĸ�thread����
							// threadid
							threads_.erase(threadid); // ע�ⲻҪ����std::this_thread_get_id()
							curThreadSize_--;
							idleThreadSize_--;

							std::cout << "cached����threadid:" << std::this_thread::get_id() << " exit!" << std::endl;;
							return;
						}
					}
				}

				else
				{
					notEmpty_.wait(lock);
				}

				// �̳߳�Ҫ������Դ
				//if (!isPoolRunning_) {
				//	threads_.erase(threadid); // ע�ⲻҪ����std::this_thread_get_id()
				//	//curThreadSize_--;
				//	//idleThreadSize_--;  ��ỹ�б�Ҫά�������𣬰��̳߳���Դ���¾Ϳ���

				//	std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;;
				//	exitCond_.notify_all();
				//	return;
				//}
			}

			idleThreadSize_--;

			std::cout << "tid: " << std::this_thread::get_id() << "��ȡ����ɹ�" << std::endl;

			task = taskQue.front();
			taskQue.pop();
			taskSize_--;

			// �����Ȼ��ʣ�����񣬼���֪ͨ�������߳�ִ������
			if (taskQue.size() > 0) {
				notEmpty_.notify_all();
			}

			// ִ����һ�����񣬵ý���֪ͨ
			notFull_.notify_all();
		}// �����ͷŵ�!!!!!!!!!!!!!!!������

		// ���ش��������ܹ�
		if (task != nullptr) {
			task->exec();
		}

		idleThreadSize_++;
		// �����߳�ִ���������ʱ��
		lastTime = std::chrono::high_resolution_clock().now();
	}


}

bool ThreadPool::checkRunningState() const {
	return isPoolRunning_;
}

//////////// �̷߳���ʵ��

int Thread::generateId_ = 0;

// �̹߳���
Thread::Thread(ThreadFunc func) : func_(func), threadId_(generateId_++) {}

// �߳�����
Thread::~Thread() {}
// �����߳�
void Thread::start() {
	// ����һ���߳���ִ��һ���̺߳���
	std::thread t(func_, threadId_);// C++11��˵���̶߳���t���̺߳���func_
	t.detach(); // ���÷����߳� pthread.detach
}

int Thread::getId() const {
	return threadId_;
}

/////////////// Task����ʵ��
Task::Task()
	:result_(nullptr)
{}
void Task::exec() {
	//std::cout << "exec" << std::endl;
	if (result_ != nullptr) {
		result_->setVal(run());
	}
}

void Task::setResult(Result* res) {
	result_ = res;
}

////////////// Result������ʵ��
Result::Result(std::shared_ptr<Task> task, bool isValid)
	:isValid_(isValid)
	, task_(task)
{
	task_->setResult(this);
}

// ����һ��setVal��������ȡ����ִ����ķ���ֵ��
void Result::setVal(Any any) {
	this->any_ = std::move(any);
	sem_.post();
}
// �������get�������û���������Ŵ��ȡtask�ķ���ֵ
Any Result::get() {
	if (!isValid_) {
		return "";
	}
	sem_.wait();
	return std::move(any_);
}

