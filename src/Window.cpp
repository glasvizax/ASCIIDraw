#include "Window.h"

#include <windows.h>

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
		cfi.dwFontSize.X = 8;
		cfi.dwFontSize.Y = 8;
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

void Window::init()
{
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr)
	{
		//TODO handle
		return;
	}

	if (!setConsoleSizeAndFont(hOut, m_size.x, m_size.y))
	{
		// TODO handle
		return;
	}
}
