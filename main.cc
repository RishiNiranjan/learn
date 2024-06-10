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
#include <vector>

#define BUFSIZE_1MB 1048576

typedef struct chunk
{
    std::vector<char> buf;
    unsigned int size = 0;    
} Chunk;

std::mutex mtx;
std::atomic_bool eof = false;
std::condition_variable cv;

std::list<Chunk> ll_Chunks;

void copy(std::ifstream &inputFile)
{
    Chunk chunk;

    while (!eof)
    {
        chunk.buf.reserve(BUFSIZE_1MB);

        inputFile.read(chunk.buf.data(), BUFSIZE_1MB);
        chunk.size = inputFile.gcount();

        if(inputFile.bad())
        {
            std::cerr<<"Copy failed\n";
            return;
        }

        {
            std::scoped_lock<std::mutex> mm(mtx);
            ll_Chunks.push_back(std::move(chunk));
            
            if (inputFile.eof())
                eof = true;
        }

        cv.notify_one();
    }
}

void paste(std::ofstream &outputFile)
{
    while (true)
    {
        std::unique_lock<std::mutex> mm(mtx);

        while (ll_Chunks.size())
        {
            mm.unlock();
                outputFile.write(ll_Chunks.front().buf.data(), ll_Chunks.front().size);
                
                if(outputFile.bad())
                {
                    std::cerr<<"Writing to file failed\n";
                    return;
                }
            mm.lock();
            
            ll_Chunks.pop_front();
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