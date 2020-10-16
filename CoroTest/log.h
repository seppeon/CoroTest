#pragma once
#include <iostream>

template <typename ... Args>
void RawWriteLine(Args const & ... args) noexcept
{
	((std::cout << args), ...);
	std::cout << "\n";
}

template <typename ... Args>
void Title(Args const & ... args) noexcept
{
	std::cout << "---------------------------------\n";
	std::cout << "Title\t";
	((std::cout << args), ...);
	std::cout << "\n";
	std::cout << "---------------------------------\n";
}

template <typename ... Args>
void Log(Args const & ... args) noexcept
{
	std::cout << "LOG\t";
	((std::cout << args), ...);
	std::cout << "\n";
}

template <typename ... Args>
void WriteLine(Args const & ... args) noexcept
{
	std::cout << "\t";
	((std::cout << args), ...);
	std::cout << "\n";
}

template <typename ... Args>
void Assert(bool value, Args const & ... args) noexcept
{
	RawWriteLine(value ? "PASS\t" : "FAIL\t", args...);
}
#define ASSERT(result, expected) Assert(result == expected, __func__, ":", __LINE__, " ", #result " == " #expected, "\n\t[ result = ", result, ", expected = ", expected, " ]")
#define LOG_MEM_FN() WriteLine(__func__, ": ", this)