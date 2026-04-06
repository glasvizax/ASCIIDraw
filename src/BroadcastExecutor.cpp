#include "BroadcastExecutor.h"

BroadcastExecutor::BroadcastExecutor(unsigned int thread_count, std::atomic<bool>& outer_end_flag)
	:
	m_thread_count(thread_count),
	m_current_thread_count(thread_count),
	m_outer_end_flag(outer_end_flag)
{
	m_threads.reserve(m_thread_count);
	for (unsigned int i = 0; i < m_thread_count; ++i)
	{
		m_threads.emplace_back([&, i]()
			{
				int last_task_id = 0;
				while (true)
				{
					void* wrapper = nullptr;
					Callback current_task = nullptr;
					bool stop = false;
					{
						int current_task_id = 0;
						std::unique_lock lk(m_task_mtx);
						m_task_cv.wait(lk, [&]()
							{
								current_task_id = m_task_id.load();
								stop = m_outer_end_flag.load() || m_inner_end_flag.load();

								return stop || current_task_id != last_task_id;
							});
						last_task_id = current_task_id;
						current_task = m_current_task;
						wrapper = m_wrapper;
					}

					if (stop)
					{
						return;
					}

					current_task(wrapper, i, m_current_thread_count);

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
				}
			}
		);
	}
}

void BroadcastExecutor::wait()
{
	bool stop = false;
	{
		std::unique_lock lk(m_current_task_done_mtx);
		m_current_task_done_cv.wait(lk, [&]()
			{
				stop = m_outer_end_flag.load() || m_inner_end_flag.load();
				return m_current_task_done || stop;
			}
		);
	}

	if (stop)
	{
		return;
	}
	m_ready.store(true);
}

void BroadcastExecutor::notifyAll()
{
	//m_inner_end_flag.store(true);
	m_current_task_done_cv.notify_all();
	m_task_cv.notify_all();
}

BroadcastExecutor::~BroadcastExecutor()
{
	m_inner_end_flag.store(true);
	notifyAll();

	for (auto& th : m_threads)
	{
		if (th.joinable())
		{
			th.join();
		}
	}
}

