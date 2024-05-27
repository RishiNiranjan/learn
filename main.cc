#include <cstring>
#include <iostream>
#include <fstream>
#include <thread>
#include <cerrno>
#include <array>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <list>
#include <chrono>
#include <benchmark/benchmark.h>

#define BUFSIZE_1MB 1048576

typedef struct chunk
{
    std::array<char, BUFSIZE_1MB> chk;
    unsigned int size = 0;    
} chunk;

std::mutex mtx;
std::atomic_bool eof = false;
std::condition_variable cv;

std::list<chunk> buffer;

void copy(std::ifstream &inputFile)
{
    chunk chk;

    while (!eof)
    {
        inputFile.read(chk.chk.data(), BUFSIZE_1MB);
        chk.size = inputFile.gcount();

        {
            std::scoped_lock<std::mutex> mm(mtx);
            buffer.push_back(chk);
        }

        if (inputFile.eof())
            eof = true;

        cv.notify_one();
    }
}

void paste(std::ofstream &outputFile)
{
    int chunkIndex = 0;

    while (true)
    {
        std::unique_lock<std::mutex> mm(mtx);

        while (buffer.size())
        {
            mm.unlock();
            outputFile.write(buffer.front().chk.data(), buffer.front().size);
            mm.lock();
            buffer.pop_front();
        }

        if (eof)
            break;

        cv.wait(mm);
    }
}

void p4_copyFiles_thread(const char *source, const char *destination)
{
    std::ifstream inputFile(source);
    if (!inputFile)
    {
        std::cout << std::strerror(errno) << " Error opening file " << source << "\n";
        return;
    }

    std::ofstream outputFile(destination);
    if (!outputFile)
    {
        std::cout << std::strerror(errno) << " Error opening file " << destination << "\n";
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




// static void BM_VectorSum(benchmark::State& state) {
//     std::string ss("hel");
//     std::string ss1("hel");
//     for (auto _ : state) {
//         p4_copyFiles_thread("input1.txt", "output.txt");
//     }
// }

// // Register the benchmark
// BENCHMARK(BM_VectorSum);

// // Main function to run the benchmarks
// BENCHMARK_MAIN();