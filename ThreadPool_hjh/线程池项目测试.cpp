#include<iostream>
#include<chrono>
#include<thread>
#include"threadpool.h"

/*
��Щ������Ҫ��ȡ�߳�ִ�з���ֵ
1+ ������ +30000
thread 1 1+...10000
thread 2 10001+...+..

mainthread �ٰѷ���ֵ���
*/

using uLong = unsigned long long;

class MyTask :public Task
{
public:
	MyTask(int begin, int end)
		:begin_(begin)
		, end_(end) {}
	// ����һ����ô���run�ķ���ֵ����ʾ��������
	// Java python  Objectʱ�������������͵Ļ���
	// C++17 Any����
	// �����������result���ƣ�
	Any run() {//�̴߳���
		std::cout << "tid: " << std::this_thread::get_id() << "begin!" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		uLong sum = 0;
		for (uLong i = begin_; i <= end_; ++i) {
			sum += i;
		}
		std::cout << "tid: " << std::this_thread::get_id() << "end!" << std::endl;
		//std::cout << "hello" << sum << std::endl;
		return sum;
	}

private:
	int begin_;
	int end_;
};
int main() {
	{
		ThreadPool pool;
		pool.setMode(PoolMode::MODE_CACHED);
		pool.start(2);
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res4 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res5 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		uLong sum1 = res1.get().cast_<uLong>();
		std::cout << sum1 << std::endl;
	}
	std::cout << "main over!" << std::endl;
#if 0
	// ���⣺threadpool���������Ժ���ô���̳߳���ص��߳���Դ���գ���Ϊ���ڵ�������
	{
		ThreadPool pool;

		// Ҫ��֤startǰִ��setMode
		pool.setMode(PoolMode::MODE_CACHED);
		pool.start(4);

		// �����������result���ƣ�ͬ��
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

		// ����task��ִ���꣬task����û�ˣ�������task�����Result����Ҳû��
		// // ����һ��Any����ôת��Ϊ��������
		uLong sum1 = res1.get().cast_<uLong>();
		uLong sum2 = res2.get().cast_<uLong>();
		uLong sum3 = res3.get().cast_<uLong>();

		/*std::cout << sum1 << std::endl;
		std::cout << sum1 << std::endl;
		std::cout << sum1 << std::endl;*/

		// Master - Slave �߳�ģ��
		// Master�߳������ֽ�����Ȼ�������Slave�̷߳�������
		// �ȴ����Slave�߳�ִ�������񣬷��ؽ��
		// Master�̺߳ϲ����������������
		uLong sum = 0;
		for (int i = 0; i <= 300000000; ++i) {
			sum += i;
		}
		std::cout << sum << std::endl;
		std::cout << (sum1 + sum2 + sum3) << std::endl;
		/*pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());*/
	}
	// ������������ţ����̱߳���������������
	// exitCond_.wait(lock, [&]()->bool{return threads_.size()==0; });û�˻���

	getchar();
#endif

	//std::this_thread::sleep_for(std::chrono::seconds(5));
}