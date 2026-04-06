#pragma once
 
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <iostream>

// usage: 
// BroadcastExecutor exec(amounts of threads, flag that marks the end);
// exec.push([...](unsigned int thread_index, unsigned int thread_count){...}); - push task for threads
// ...
// exec.wait(); waiting until end of task received by push()
// --- OR --- 
// exec.pushSync([...](unsigned int thread_index, unsigned int thread_count){...}); - the current thread also participates in the calculations
class BroadcastExecutor
{
	template <typename Lambda>
	struct LambdaWrapper
	{
		Lambda lambda;

		static void call(void* self, unsigned int index, unsigned int threads_count)
		{
			reinterpret_cast<LambdaWrapper*>(self)->lambda(index, threads_count);
		}
	};

	using Callback = void(*)(void*, unsigned int, unsigned int);

public:
	// thread_count - threads count, outer_end_flag - flag for stop
	BroadcastExecutor(unsigned int thread_count, std::atomic<bool>& outer_end_flag);

	// push task for threads
	template<typename Lambda>
	void push(const Lambda& task);

	// push task for threads, the current thread also participates in the calculations, don't call wait() after !!!
	template<typename Lambda>
	void pushSync(const Lambda& task);

	// waiting until end of task received by push()
	void wait();

	~BroadcastExecutor();

	// threads count
	const int m_thread_count;

private:

	void notifyAll();

	template<typename Lambda>
	bool pushImpl(const Lambda& task);

	int m_current_thread_count;

	Callback m_current_task = nullptr;
	void* m_wrapper = nullptr;

	std::atomic<bool>& m_outer_end_flag;
	std::atomic<bool> m_inner_end_flag{ false };

	std::atomic<bool> m_ready = true;

	bool m_current_task_done = false;
	std::condition_variable m_current_task_done_cv;
	std::mutex m_current_task_done_mtx;

	std::atomic<int> m_completed_counter = 0;
	std::vector<std::thread> m_threads;
	std::mutex m_task_mtx;
	std::condition_variable m_task_cv;

	std::atomic<int> m_task_id = 0;
};

template<typename Lambda>
inline void BroadcastExecutor::push(const Lambda& task)
{
	if (!pushImpl(task))
	{
		return;
	}
}

template<typename Lambda>
inline void BroadcastExecutor::pushSync(const Lambda& task)
{
	m_current_thread_count++;
	if(!pushImpl(task))
	{
		m_current_thread_count--;
		return;
	}

	void* wrapper = nullptr;
	Callback current_task = nullptr;
	{
		std::scoped_lock lk(m_task_mtx);
		current_task = m_current_task;
		wrapper = m_wrapper;
	}

	if (m_outer_end_flag.load() || m_inner_end_flag.load())
	{
		return;
	}

	current_task(wrapper, m_current_thread_count - 1, m_current_thread_count);

	if (m_completed_counter.fetch_add(1) == m_current_thread_count - 1)
	{
		{
			std::scoped_lock lk(m_task_mtx);
			m_current_task = nullptr;
			m_completed_counter.store(0);
		}
		{
			std::scoped_lock lk(m_current_task_done_mtx);
			m_current_task_done = true;
		}

		m_current_task_done_cv.notify_one();
	}
	wait();
	m_current_thread_count--;
}

template<typename Lambda>
inline bool BroadcastExecutor::pushImpl(const Lambda& task)
{
	if (!m_ready.exchange(false))
	{
		std::cerr << "not ready" << std::endl;
		return false;
	}

	// FUN ...
	static char static_buf[sizeof(LambdaWrapper<Lambda>)];
	static bool initialized = false;

	if (initialized)
	{
		reinterpret_cast<LambdaWrapper<Lambda>*>(static_buf)->~LambdaWrapper<Lambda>();
	}

	new(static_buf) LambdaWrapper<Lambda>{task};
	initialized = true;
	// ...

	{
		std::scoped_lock lk(m_current_task_done_mtx);
		m_current_task_done = false;
	}
	{
		std::scoped_lock lk(m_task_mtx);
		m_current_task = reinterpret_cast<Callback>(&LambdaWrapper<Lambda>::call);
		m_wrapper = static_cast<void*>(&static_buf);
		m_completed_counter.store(0);
		++m_task_id;
	}
	m_task_cv.notify_all();
	return true;

}

