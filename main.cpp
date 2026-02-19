#include <windows.h>
#include <stdio.h>
#include <string>
#include <iostream>

struct Framebuffer 
{
    std::string buffer;

};

int main(int argc, char* argv[])
{

    Framebuffer fb;
    
    while (true)
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        int columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

        static std::string testbuf;
        testbuf.assign(columns * rows, 'a');

        testbuf[0] = 'b';

        std::cout << "\x1b[H";

        for (int y = 0; y < rows; ++y) {
            std::cout.write(&testbuf[y * columns], columns);
            std::cout << '\n';
        }

        //std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return 0;
}