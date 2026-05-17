#include "ConsoleWindow.h"

#include <iostream>
#include <cassert>

#include <windows.h>

#include "IntensityUtils.h"

bool setConsoleSizeAndFont(HANDLE hOut, int width, int height)
{
	DWORD mode = 0;
	if (!GetConsoleMode(hOut, &mode))
	{
		return false;
	}

	mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	if (!SetConsoleMode(hOut, mode))
	{
		return false;
	}

	CONSOLE_FONT_INFOEX cfi = {};
	cfi.cbSize = sizeof(cfi);

	if (GetCurrentConsoleFontEx(hOut, FALSE, &cfi))
	{
		cfi.dwFontSize.X = 5;
		cfi.dwFontSize.Y = 5;
		wcscpy_s(cfi.FaceName, L"Consolas");

		if (!SetCurrentConsoleFontEx(hOut, FALSE, &cfi))
		{
			return false;
		}
	}

	CONSOLE_SCREEN_BUFFER_INFO csbi = {};
	if (!GetConsoleScreenBufferInfo(hOut, &csbi))
	{
		return false;
	}

	SMALL_RECT currentWindow = csbi.srWindow;
	COORD currentBuffer = csbi.dwSize;

	SMALL_RECT tinyWindow = { 0, 0, 0, 0 };
	if (currentWindow.Right - currentWindow.Left + 1 > width ||
		currentWindow.Bottom - currentWindow.Top + 1 > height)
	{
		if (!SetConsoleWindowInfo(hOut, TRUE, &tinyWindow))
		{
			return false;
		}
	}

	COORD newBufferSize;
	newBufferSize.X = width;
	newBufferSize.Y = height;

	if (!SetConsoleScreenBufferSize(hOut, newBufferSize))
	{
		return false;
	}

	SMALL_RECT newWindow;
	newWindow.Left = 0;
	newWindow.Top = 0;
	newWindow.Right = width - 1;
	newWindow.Bottom = height - 1;

	if (!SetConsoleWindowInfo(hOut, TRUE, &newWindow))
	{
		return false;
	}

	return true;
}

void ConsoleWindow::init(std::atomic<bool>& stop_flag)
{
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr)
	{
		//TODO handle
		assert(false);
		return;
	}

	if (!setConsoleSizeAndFont(hOut, m_size.x, m_size.y))
	{
		// TODO handle
		assert(false);
		return;
	}
	std::ios::sync_with_stdio(false);
	m_main_framebuffer.init(m_size);
	m_z_framebuffer.init(m_size);

	m_input_thread = std::thread(&ConsoleWindow::inputThread, this, std::ref(stop_flag));
}

void ConsoleWindow::draw()
{
	std::cout << "\x1b[H";
	int n = m_size.x * m_size.y;
	std::cout.write(m_main_framebuffer.getBuffer(), n);
}

void ConsoleWindow::draw(BroadcastExecutor& exec)
{

}

void ConsoleWindow::clear()
{
	m_main_framebuffer.clear(m_clear_symbol);
	m_z_framebuffer.clear(1.0f);
}

void ConsoleWindow::clear(BroadcastExecutor& exec)
{
	m_main_framebuffer.clear(m_clear_symbol, exec);
	m_z_framebuffer.clear(1.0f, exec);
}

void ConsoleWindow::destroy() 
{
	if(m_input_thread.joinable())
	{
		m_input_thread.join();
	}
}

void ConsoleWindow::inputThread(std::atomic<bool>& stop_flag)
{
	while (!stop_flag.load())
	{
		wchar_t c = _getwch();
		std::scoped_lock lock(m_input_mtx);
		m_input_queue.push(c);
	}
}

wchar_t ConsoleWindow::getLastInputKeyWChar()
{
	wchar_t key = L'\0';
	{
		std::lock_guard<std::mutex> lock(m_input_mtx);
		if (!m_input_queue.empty())
		{
			key = m_input_queue.front();
			m_input_queue.pop();
		}
	}
	return key;
}
