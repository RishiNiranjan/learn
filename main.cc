#include <cstring>
#include <iostream>
#include <fstream>
#include <thread>
#include <cerrno>
#include <array>
#include <mutex>
#include <condition_variable>
#include <chrono>

#define BUFSIZE_1MB 1048576
#define NOOFCHUNKS 10

typedef struct chunk
{
    std::array<char, BUFSIZE_1MB> buf;
    unsigned int size = 0;
    bool eof = 0;
} Chunk;

bool eof = false;
std::mutex mtx;
std::condition_variable cv;

std::array<Chunk, NOOFCHUNKS> _buffer;
int front = 0, rear = 1;

bool increment(int& x, int y)
{
    if((x+1)%NOOFCHUNKS == y)
        return false;

    x = (x+1)%NOOFCHUNKS;
    return true;
}

void copy(std::ifstream &inputFile)
{
    while (true)
    {
        bool status = false;

        {
            std::unique_lock<std::mutex> mm(mtx);
            if(inputFile.eof())
            {
                eof = true;
                break;
            }
            status = cv.wait_for(mm, std::chrono::seconds(2), [&](){ return increment(rear, front);}); 
        }

        if(status)
        {
            inputFile.read(_buffer[rear].buf.data(), BUFSIZE_1MB);
            _buffer[rear].size = inputFile.gcount();
            _buffer[rear].eof = inputFile.eof();

            if(inputFile.bad())
            {
                std::cerr<<"Copy failed\n";
                return;
            }
        }
    }
}

void paste(std::ofstream &outputFile)
{
    while (true)
    {
        bool status = true;
        
        {
            std::unique_lock<std::mutex> mm(mtx);
        
            if(eof)
                front = (front+1)%NOOFCHUNKS;
            else
                status = cv.wait_for(mm, std::chrono::seconds(2), [&](){return increment(front, rear);});
        }
        
        if(status) 
        {
            outputFile.write(_buffer[front].buf.data(), _buffer[front].size);
        
            if(outputFile.bad())
            {
                std::cerr<<"Writing to file failed\n";
                return;
            }

            if(_buffer[front].eof)
                break;
        }
    }
}

void p4_copyFiles_thread(const char *source, const char *destination)
{
    std::ifstream inputFile(source);
    if (!inputFile)
    {
        std::cerr << std::strerror(errno) << " Error opening file " << source << "\n";
        return;
    }

    std::ofstream outputFile(destination);
    if (!outputFile)
    {
        std::cerr << std::strerror(errno) << " Error opening file " << destination << "\n";
        return;
    }

    std::thread copyT(copy, std::ref(inputFile));
    std::thread pasteT(paste, std::ref(outputFile));

    copyT.join();
    pasteT.join();
}


int main(int argc, char* argv[])
{
    if(argc < 3)
        return 0;
    
    srand(static_cast<unsigned>(time(0)));
    auto start = std::chrono::high_resolution_clock::now();
    
    p4_copyFiles_thread(argv[1], argv[2]);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = end - start;
    std::cout << "Time taken " <<duration.count() << " ms" << std::endl;

    return 0;
}






