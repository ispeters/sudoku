// Sudoku.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <algorithm>
#include <array>
#include <bitset>
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
    if (auto hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        hOut != INVALID_HANDLE_VALUE)
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
    T&           state;
    OffsetOf     offset_of;
    mutable char unused[10]{};

public:
    Region(T& state, size_t index) noexcept : state(state), offset_of{index}
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

    char const* unused_numbers() const noexcept
    {
        if (unused[0] == '\0')
        {
            int next = 0;
            for (char i = '1'; i <= '9'; ++i)
            {
                if (!contains(i))
                {
                    unused[next++] = i;
                }
            }

            while (next < 9)
            {
                unused[next++] = ' ';
            }
        }

        return unused;
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

template <typename T>
std::ostream& operator<<(std::ostream&               out,
                         Region<T, RowOffset> const& row) noexcept
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
std::ostream& operator<<(std::ostream&                  out,
                         Region<T, ColumnOffset> const& col) noexcept
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
std::ostream& operator<<(std::ostream&               out,
                         Region<T, BoxOffset> const& box) noexcept
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

using RowRef  = Region<Board, RowOffset>;
using RowView = Region<Board const, RowOffset>;

using ColRef  = Region<Board, ColumnOffset>;
using ColView = Region<Board const, ColumnOffset>;

using BoxRef  = Region<Board, BoxOffset>;
using BoxView = Region<Board const, BoxOffset>;

std::ostream& operator<<(std::ostream& out, Board const& board) noexcept;

class Board
{
    std::array<char, 81> state;
    size_t               solveCalls{};

    std::array<std::bitset<9>, 9> rowBits;
    std::array<std::bitset<9>, 9> colBits;
    std::array<std::bitset<9>, 9> boxBits;

    static bool ValidState(std::array<char, 81> const& state) noexcept
    {
        return std::all_of(state.begin(), state.end(), [](wchar_t ch) noexcept {
            return (ch >= L'1' && ch <= L'9') || ch == L' ';
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

    std::bitset<9> cell_bits(size_t index) const noexcept
    {
        if (state[index] != ' ')
        {
            return {0x1ff};
        }
        else
        {
            return rowBits[row_of(index)] | colBits[col_of(index)]
                   | boxBits[box_of(index)];
        }
    }

    size_t next_index() const noexcept
    {
        size_t index = 81;
        size_t count = 0;

        for (size_t i = 0; i < 81; ++i)
        {
            auto cellBits = cell_bits(i);

            if (cellBits.count() == 9)
            {
                if (state[i] == ' ')
                {
                    // no viable numbers to fill a blank
                    return 82;
                }

                continue;
            }

            if (cellBits.count() > count)
            {
                count = cellBits.count();
                index = i;
            }
        }

        return index;
    }

    void redraw(size_t index) const noexcept
    {
        auto rowIndex = row_of(index);
        auto colIndex = col_of(index);

        auto screenY = rowIndex + (rowIndex / 3) + 1;
        auto screenX = colIndex + (colIndex / 3) + 1;

        std::printf(ESC "[%zu;%zuH%c", screenY, screenX, state[index]);

        /*
        std::printf(ESC "[%zu;13H%s", screenY, thisRow.unused_numbers());

        {
            char const* c = thisCol.unused_numbers();
            for (int i = 13; *c != '\0'; ++c, ++i)
            {
                std::printf(ESC "[%d;%zuH%c", i, screenX, *c);
            }
        }
        */
    }

    bool solve_faster(size_t index) noexcept
    {
        ++solveCalls;

        if (index >= 81)
        {
            return index == 81;
        }

        assert(state[index] == ' ');

        auto cellBits = cell_bits(index);

        for (size_t i = 0; i < 9; ++i)
        {
            if (cellBits[i])
            {
                continue;
            }

            state[index] = static_cast<char>('1' + i);
            rowBits[row_of(index)].set(i);
            colBits[col_of(index)].set(i);
            boxBits[box_of(index)].set(i);

            redraw(index);

            if (solve_faster(next_index()))
            {
                return true;
            }

            state[index] = ' ';
            rowBits[row_of(index)].reset(i);
            colBits[col_of(index)].reset(i);
            boxBits[box_of(index)].reset(i);

            redraw(index);
        }

        return false;
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

            redraw(index);

            if (solve(index + 1))
            {
                return true;
            }

            state[index] = ' ';

            redraw(index);
        }

        return false;
    }

public:
    Board(std::string_view initialState) noexcept
    {
        assert(initialState.size() == 81);

        std::copy_n(initialState.begin(), initialState.size(), state.begin());

        assert(ValidState(state));

        for (size_t i = 0; i < 9; ++i)
        {
            auto row = this->row(i);
            auto col = this->col(i);
            auto box = this->box(i);

            for (size_t j = 0; j < 9; ++j)
            {
                if (row[j] != ' ')
                {
                    rowBits[i].set(row[j] - '1');
                }

                if (col[j] != ' ')
                {
                    colBits[i].set(col[j] - '1');
                }

                if (box[j] != ' ')
                {
                    boxBits[i].set(box[j] - '1');
                }
            }
        }
    }

    Board visualize_cell_bits() const noexcept
    {
        std::array<char, 81> board;

        for (size_t i = 0; i < 81; ++i)
        {
            auto possibleNumbers =
                '0' + static_cast<char>(9 - cell_bits(i).count());

            if (possibleNumbers == '0')
            {
                board[i] = ' ';
            }
            else
            {
                board[i] = possibleNumbers;
            }
        }

        return {{board.data(), board.size()}};
    }

    size_t solve() noexcept
    {
        solveCalls = 0;

        solve(0);

        return solveCalls;
    }

    size_t solve_faster() noexcept
    {
        solveCalls = 0;

        solve_faster(next_index());

        return solveCalls;
    }

    char operator[](size_t index) const noexcept
    {
        return state[index];
    }

    char& operator[](size_t index) noexcept
    {
        return state[index];
    }

    RowRef row(size_t index) noexcept
    {
        assert(index < 9);

        return {*this, index};
    }

    RowView row(size_t index) const noexcept
    {
        assert(index < 9);

        return {*this, index};
    }

    ColRef col(size_t index) noexcept
    {
        assert(index < 9);

        return {*this, index};
    }

    ColView col(size_t index) const noexcept
    {
        assert(index < 9);

        return {*this, index};
    }

    BoxRef box(size_t index) noexcept
    {
        assert(index < 9);

        return {*this, index};
    }

    BoxView box(size_t index) const noexcept
    {
        assert(index < 9);

        return {*this, index};
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

    Sudoku::Board board("     2  7"
                        "75   8 3 "
                        "38  15 6 "
                        "   8   1 "
                        "8659 1742"
                        " 7   6   "
                        " 4 25  96"
                        " 9 6   21"
                        "2  1     ");

    std::cout << ESC "[1m" << board << ESC "[0m\n";

    // std::cout << board.visualize_cell_bits() << '\n';

    std::printf(ESC "[s");

    std::getchar();

    auto start = std::chrono::system_clock::now();

    auto count = board.solve_faster();

    auto end = std::chrono::system_clock::now();

    std::printf(ESC "[u");

    std::cout << "Solve called " << count << " times\n";
    std::cout << "Solution took "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end
                                                                       - start)
                     .count()
              << "ms\n";

    std::getchar();

    return 0;
}
