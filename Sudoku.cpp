// Sudoku.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>

#include <Windows.h>

#define ESC "\x1B"

namespace Sudoku {

static constexpr char ClearScreen[] = ESC "[2J";

static void InitVirtualTerminalProcessing()
{
	if (auto hOut = GetStdHandle(STD_OUTPUT_HANDLE); hOut != INVALID_HANDLE_VALUE)
	{
		if (DWORD dwMode{}; GetConsoleMode(hOut, &dwMode))
		{
			dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

			if (SetConsoleMode(hOut, dwMode))
			{
				return;
			}
		}
	}

	std::terminate();
}

template <typename T, typename OffsetOf>
class Region
{
	T& state;
	OffsetOf offset_of;

public:
	Region(T& state, size_t index) noexcept
		: state(state), offset_of{ index }
	{
		assert(index < 9);
	}

	char operator[](size_t index) const noexcept
	{
		assert(index < 9);

		return state[offset_of(index)];
	}

	template <typename C = std::enable_if_t<!std::is_const_v<T>, char>>
	C& operator[](size_t index) noexcept
	{
		assert(index < 9);

		return state[offset_of(index)];
	}

	bool contains(char number) const noexcept
	{
		for (size_t i = 0; i < 9; ++i)
		{
			if (operator[](i) == number)
			{
				return true;
			}
		}

		return false;
	}
};

struct RowOffset
{
	size_t row;

	constexpr size_t operator()(size_t index) const noexcept
	{
		return row * 9 + index;
	}
};

struct ColumnOffset
{
	size_t col;

	constexpr size_t operator()(size_t index) const noexcept
	{
		return index * 9 + col;
	}
};

struct BoxOffset
{
	size_t boxRow;
	size_t boxCol;

	constexpr BoxOffset(size_t index) noexcept
		: boxRow(index / 3), boxCol(index % 3) 
	{
	}

	constexpr size_t operator()(size_t index) const noexcept
	{
		return boxRow * 27 + boxCol * 3 + (index / 3 * 9) + (index % 3);
	}
};

using RowRef = Region<std::string, RowOffset>;
using RowView = Region<std::string const, RowOffset>;

using ColRef = Region<std::string, ColumnOffset>;
using ColView = Region<std::string const, ColumnOffset>;

using BoxRef = Region<std::string, BoxOffset>;
using BoxView = Region<std::string const, BoxOffset>;

template <typename T>
std::ostream& operator<<(std::ostream& out, Region<T, RowOffset> const& row) noexcept
{
	for (size_t i = 0; i < 9; ++i)
	{
		out << row[i];

		if (i == 2 || i == 5)
		{
			out << ESC "(0x" ESC "(B";
		}
	}

	return out;
}

template <typename T>
std::ostream& operator<<(std::ostream& out, Region<T, ColumnOffset> const& col) noexcept
{
	for (size_t i = 0; i < 9; ++i)
	{
		out << col[i] << '\n';

		if (i == 2 || i == 5)
		{
			out << ESC "(0q" ESC "(B\n";
		}
	}

	return out;
}

template <typename T>
std::ostream& operator<<(std::ostream& out, Region<T, BoxOffset> const& box) noexcept
{
	for (size_t i = 0; i < 9; ++i)
	{
		out << box[i];

		if (i == 2 || i == 5)
		{
			out << '\n';
		}
	}

	return out;
}

class Board;

std::ostream& operator<<(std::ostream& out, Board const& board) noexcept;

class Board
{
	std::string state;
	size_t solveCalls{};

	static bool ValidState(std::string const& state) noexcept
	{
		return std::all_of(state.begin(), state.end(), [](wchar_t ch) noexcept
		{
			return (ch >= L'1' && ch <= L'9')
				|| ch == L' ';
		});
	}

	static size_t row_of(size_t index) noexcept
	{
		return index / 9;
	}

	static size_t col_of(size_t index) noexcept
	{
		return index % 9;
	}

	static size_t box_of(size_t index) noexcept
	{
		auto boxRow = row_of(index) / 3;
		auto boxCol = col_of(index) / 3;

		return boxRow * 3 + boxCol;
	}

	bool solve(size_t index) noexcept
	{
		++solveCalls;

		if (index >= 81)
		{
			return true;
		}

		if (state[index] != ' ')
		{
			return solve(index + 1);
		}

		auto rowIndex = row_of(index);
		auto colIndex = col_of(index);

		auto screenY = rowIndex + (rowIndex / 3) + 1;
		auto screenX = colIndex + (colIndex / 3) + 1;

		auto thisRow = row(rowIndex);
		auto thisCol = col(colIndex);
		auto thisBox = box(box_of(index));

		for (char i = '1'; i <= '9'; ++i)
		{
			if (thisRow.contains(i))
			{
				continue;
			}

			if (thisCol.contains(i))
			{
				continue;
			}

			if (thisBox.contains(i))
			{
				continue;
			}

			state[index] = i;

			std::printf(ESC "[%zu;%zuH%c", screenY, screenX, i);

			if (solve(index + 1))
			{
				return true;
			}

			state[index] = ' ';

			std::printf(ESC "[%zu;%zuH ", screenY, screenX);
		}

		return false;
	}

public:
	Board(char const * initialState) noexcept
		: state(initialState)
	{
		assert(state.size() == 81);
		assert(ValidState(state));
	}

	size_t solve() noexcept
	{
		solveCalls = 0;

		solve(0);

		return solveCalls;
	}

	RowRef row(size_t index) noexcept
	{
		assert(index < 9);

		return { state, index };
	}

	RowView row(size_t index) const noexcept
	{
		assert(index < 9);

		return { state, index };
	}

	ColRef col(size_t index) noexcept
	{
		assert(index < 9);

		return { state, index };
	}

	ColView col(size_t index) const noexcept
	{
		assert(index < 9);

		return { state, index };
	}

	BoxRef box(size_t index) noexcept
	{
		assert(index < 9);

		return { state, index };
	}

	BoxView box(size_t index) const noexcept
	{
		assert(index < 9);

		return { state, index };
	}
};

std::ostream& operator<<(std::ostream& out, Board const& board) noexcept
{
	for (size_t i = 0; i < 9; ++i)
	{
		out << board.row(i) << '\n';

		if (i == 2 || i == 5)
		{
			out << ESC "(0qqqnqqqnqqq" ESC "(B\n";
		}
	}

	return out;
}

} // namespace Sudoku

int main()
{
	Sudoku::InitVirtualTerminalProcessing();

	std::printf(Sudoku::ClearScreen);

	Sudoku::Board board(
		"5    8  3"
		" 2    8  "
		" 16 4  9 "
		"   18    "
		"2 8   1 7"
		"    62   "
		" 4  5 27 "
		"  9    6 "
		"6  3    9"
		);

	std::cout << ESC "[1m" << board << ESC "[0m\n";

	std::printf(ESC "[s");

	auto start = std::chrono::system_clock::now();

	auto count = board.solve();

	auto end = std::chrono::system_clock::now();

	std::printf(ESC "[u");

	std::cout << "Solve called " << count << " times\n";
	std::cout << "Solution took " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms\n";

	std::getchar();

    return 0;
}

