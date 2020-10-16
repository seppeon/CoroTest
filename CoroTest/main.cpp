#include "coro.h"
#include "log.h"
#include <memory>

void * m_current = nullptr;

struct None{};

struct SuspendAlways
{
    static bool await_ready() noexcept { return false; }
    static void await_suspend(default_handle) noexcept {}
    static void await_resume() noexcept {}
};

struct Continuable
{
private:
	default_handle m_continuation = nullptr;
public:
	void SetCurrent(default_handle handle) noexcept
	{
		LOG_MEM_FN();
		m_current = handle.address();
	}

    default_handle ContinueWith() noexcept
    {
		LOG_MEM_FN();
        auto output = std::exchange(m_continuation, nullptr);
		m_current = nullptr;
		return output;
    }

	default_handle ContinueWith(default_handle handle) noexcept
	{
		LOG_MEM_FN();
		return std::exchange(m_continuation, handle);
	}
};

struct YieldAwaitable
{
	Continuable & m_promise;

	bool await_ready() const noexcept
	{
		WriteLine("YieldAwaitable: ", __func__, ": ", &m_promise);
		return false;
	}

	auto await_suspend(default_handle) const noexcept
	{
		WriteLine("YieldAwaitable: ", __func__, ": ", &m_promise);
		return m_promise.ContinueWith();
	}

	void await_resume() const noexcept
	{
		WriteLine("YieldAwaitable: ", __func__, ": ", &m_promise);
	}
};

struct RootAwaitable : SuspendAlways
{
	template <typename T>
	RootAwaitable(T &&) noexcept{}
};

template <typename Promise>
struct AwaitAwaitable
{
	Promise & m_promise;

	bool await_ready() const noexcept
	{
		WriteLine("AwaitAwaitable: ", __func__, ": ", &m_promise);
		return false;
	}

	void await_suspend(default_handle handle) noexcept
	{
		WriteLine("AwaitAwaitable: ", __func__, ": ", &m_promise);
		m_promise.SetCurrent( m_promise.Handle() );
		m_promise.ContinueWith( handle );
	}

	auto await_resume() const noexcept
	{
		WriteLine("AwaitAwaitable: ", __func__, ": ", &m_promise);
		return m_promise.GetValue();
	}
};

template <typename P>
AwaitAwaitable(P)->AwaitAwaitable<P>;

template <typename T>
struct Settable;

struct SettableHasValue : Continuable
{
private:
	bool m_has_value = false;
public:
	bool HasValue() const noexcept { return m_has_value; }
	void Set() noexcept { m_has_value = true;  }
	void Clear() noexcept { m_has_value = false; }
};

template <>
struct Settable<void> : SettableHasValue
{
	auto yield_value(None) noexcept -> YieldAwaitable
	{
		LOG_MEM_FN();
		Set();
		return {*this};
	}
	void return_void() noexcept 
	{ 
		LOG_MEM_FN();
		Clear();
	}
	void GetValue() const noexcept {}
};


template <typename T>
struct Settable : SettableHasValue
{
private:
	using base = SettableHasValue;
	union { T m_value; };

    void Construct(T const & value) noexcept
    {
		std::construct_at(std::launder(&m_value), value);
		base::Set();
    }

    void Destruct() noexcept
    {
        base::Clear();
		std::destroy_at(std::launder(&m_value));
    }
public:
	Settable() noexcept
	{
		// Deterministic initialisation, not eratic, this isn't initialised memory
		// so this doesn't affect the value after Construct is called.
		memset(&m_value, 0, sizeof(m_value));
	}

	void SetValue(T const & value) noexcept
	{
		LOG_MEM_FN();
        if (HasValue()) Destruct();
		Construct(value);
	}
	
	T GetValue() noexcept
	{
		LOG_MEM_FN();
		T output{ std::move( *std::launder(&m_value) ) };
		Destruct();
		return output;
	}

	auto yield_value(T const & value) noexcept -> YieldAwaitable
	{
		LOG_MEM_FN();
		SetValue(value);
		return {*this}; 
	}

	void return_value(T const & value) noexcept
	{
		LOG_MEM_FN();
		SetValue(value);
	}
};

template <typename T, typename FinalAwaitable = YieldAwaitable>
struct Task : private default_handle
{
	struct promise_type : Settable<T>
	{
		auto Handle() noexcept
		{
			return handle_type::from_promise(*this);
		}

		Task<T, FinalAwaitable> get_return_object() noexcept
		{
			LOG_MEM_FN();
			return { Handle() };
		}

		SuspendAlways initial_suspend() noexcept
		{
			LOG_MEM_FN();
			return {};
		}

		FinalAwaitable final_suspend() noexcept
		{
			LOG_MEM_FN();
			return { *this };
		}

		void unhandled_exception() noexcept
		{
			LOG_MEM_FN();
			std::terminate();
		}

		template <typename Tsk>
		auto await_transform( Tsk && tsk ) noexcept
		{
			LOG_MEM_FN();
			return AwaitAwaitable{ std::forward<Tsk>(tsk).Promise() };
		}
	};

	using handle_type = std::coroutine_handle<promise_type>;

	handle_type & Handle() noexcept { return reinterpret_cast<handle_type&>(*this); }
	
    handle_type const & Handle() const noexcept { return reinterpret_cast<handle_type const &>(*this); }

	promise_type & Promise() noexcept { return Handle().promise(); }
	
    promise_type const & Promise() const noexcept { return Handle().promise(); }

	handle_type Detach() noexcept
	{
		return std::exchange(Handle(), nullptr);
	}

	void Destroy() noexcept
	{
		if ( address() )
		{
			WriteLine( __func__, ": ", &Promise() );
			destroy();
		}
	}

	Task() noexcept : default_handle{ nullptr }{}
    
	Task(default_handle handle) noexcept :
		default_handle{handle}
	{}

	Task(Task && task) noexcept :
		default_handle{std::exchange(std::move(task).Handle(), nullptr)}
	{
		WriteLine( __func__, ": ", &Promise() );
	}

	Task & operator=(Task && task) noexcept
	{
		WriteLine( __func__, ": ", &Promise() );
		Destroy();
		Handle() = std::exchange(std::move(task).Handle(), nullptr);
		return *this;
	}

	Task(Task const & task) noexcept = delete;
	Task & operator=(Task const & task) noexcept = delete;

	~Task()
	{ 
		Destroy();
		Handle() = nullptr;
	}
};

Task<int> test_coro0()
{
	co_return 123;
}

Task<int> test_coro1()
{
	co_return co_await test_coro0();
}

Task<int> test_coro2()
{
	co_return co_await test_coro1();
}

Task<int> test_yielding()
{
	co_yield 1;
	co_return 2;
}

struct Temp
{
	int a = 10;

	Temp()
	{
		LOG_MEM_FN();
	}
	~Temp()
	{
		LOG_MEM_FN();
	}

	Task<int> operator()() const
	{
		co_return a;
	}
};

Task<void> test_task() noexcept
{
	{
		Title("Test co_yield + co_return lvalue");
		auto fn = test_yielding();
		auto a = co_await fn;
		auto b = co_await fn;
		ASSERT(a + b, 3);
	}
	{
		Title("Test co_yield + co_return rvalue");
		auto fn = test_yielding();
		auto res =
		(
			co_await fn +
			co_await fn
		);
		ASSERT(res, 3);
	}
    co_return;
}

Task<void, RootAwaitable> Runner() noexcept
{
	co_await test_task();
}

void runner()
{
	auto run = Runner();
	m_current = run.Handle().address();
	while(m_current)
    {
		default_handle::from_address(m_current).resume();
    }
}

int main()
{
	runner();
	return 0;
}