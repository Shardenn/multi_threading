#include <iostream>
#include <Windows.h>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

typedef std::chrono::high_resolution_clock Time;
typedef std::chrono::duration<float> fsec;
typedef std::chrono::milliseconds ms;

int VerbosityLevel = 1;

std::mutex mutex;
std::atomic<int> atomicResult;

enum ThreadType
{
    winAPI_t,
    std_t,
    atomic_t
};

struct ThreadParams
{
    int* arr;
    int number;
    int elementsToProcess;
    int result;

    ThreadParams()
    {
        arr = nullptr;
        number = 0;
        elementsToProcess = 0;
        result = 0;
    }
} *pThreadParams;

void FillVectorRandom(std::vector<int> &v, int minPossible = 1, int maxPossible = 10)
{
    srand((unsigned int)time(NULL));
    for (int i = 0; i < v.size(); i++)
    {
        int a = rand() % (maxPossible - minPossible) + minPossible;
        v[i] = a;
    }
}

DWORD WINAPI FindElementsGreaterThan(void* params)
{
    mutex.lock();

    ThreadParams* p = (ThreadParams*)params;
    if (VerbosityLevel > 1)
        std::cout << " " << GetCurrentThreadId() << " is speaking.\n";

    for (int i = 0; i < p->elementsToProcess; i++)
    {
        if (VerbosityLevel > 1)
            std::cout << (p->arr)[i] << " ";

        if ((p->arr)[i] > p->number)
            p->result++;
    }

    if (VerbosityLevel > 1)
    {
        std::cout << std::endl; // to close opened line from code above
        std::cout << "Thread " << GetCurrentThreadId() << " found " <<
            p->result << " numbers greater than " << p->number << std::endl;
    }

    mutex.unlock();

    return 0;
}

void FindElementsGreaterThan_std(int* arr, int number, int elementsNumber, int& result)
{
    mutex.lock();

    if (VerbosityLevel > 1)
        std::cout << GetCurrentThreadId() << " is speaking.\n";

    result = 0;
    for (int i = 0; i < elementsNumber; i++)
    {
        if (VerbosityLevel > 1)
            std::cout << arr[i] << " ";

        if (arr[i] > number)
            result++;
    }

    if (VerbosityLevel > 1)
    {
        std::cout << std::endl;
        std::cout << GetCurrentThreadId() << " counted " << result
            << " elements greater then " << number << std::endl;
    }
    
    mutex.unlock();

    return;
}

void FindElementsGreaterThan_atomic(int *arr, int number, int elementsNumber, std::atomic<int>& results)
{
    //mutex.lock();

    if (VerbosityLevel > 1)
        std::cout << GetCurrentThreadId() << " is speaking.\n";

    int localResult = 0;
    for (int i = 0; i < elementsNumber; i++)
    {
        if (VerbosityLevel > 1)
            std::cout << arr[i] << " ";

        if (arr[i] > number)
            localResult++;
    }

    if (VerbosityLevel > 1)
    {
        std::cout << std::endl;
        std::cout << GetCurrentThreadId() << " counted " << localResult
            << " elements greater then " << number << std::endl;
    }

    //mutex.unlock();

    localResult += results.load();
    results.store(localResult);

    return;
}

int main(int argc, char **argv)
{
    ThreadType threadType = winAPI_t;
    int optThreadsNum = 1;
    int optArrayLength = 0;
    int optNumber = 0;

    std::vector<int> vec(0);

    for (int i = 1; i < argc; i++)
    {
        std::string opt = argv[i];
        if (opt == "-h")
        {
            std::cout << "USAGE: " << argv[0] << "\n" <<
                "[-n <number to search for elements greater than>]\n" <<
                "[-t <number of threads>]\n" <<
                "[-num <number of elements to generate and process>]\n" <<
                "[-threadType <winAPI | std | atomic>]\n" <<
                "[-v <verbosity level>]:\n" <<
                "v == 0 : only input info and the result with time mewasurement\n" <<
                "v == 1 : all above plus the generated array will be shown\n" <<
                "v == 2 : all above plus every thread will log his work\n";
            return 0;
        }
        else if (opt == "-n")
            optNumber = std::stoi(argv[++i]);
        else if (opt == "-t")
            optThreadsNum = std::stoi(argv[++i]);
        else if (opt == "-num")
            optArrayLength = std::stoi(argv[++i]);
        else if (opt == "-v")
            VerbosityLevel = std::stoi(argv[++i]);
        else if (opt == "-threadType")
        {
            std::string type = argv[++i];
            if (type == "winAPI")
                threadType = winAPI_t;
            else if (type == "std")
                threadType = std_t;
            else if (type == "atomic")
                threadType = atomic_t;
        }
    }

    vec.resize(optArrayLength);
    FillVectorRandom(vec);


    std::cout << "INFO: \n" << "Max number: " << optNumber << std::endl <<
        "Threads num: " << optThreadsNum << std::endl <<
        "Array length: " << optArrayLength << std::endl;

    if (VerbosityLevel > 0)
        for (int i = 0; i < vec.size(); i++)
            std::cout << vec[i] << " ";
    std::cout << std::endl;

    if (threadType == winAPI_t)
    {
        try
        {
            DWORD* dwThreadIdArray;
            HANDLE* hThreadArray;

            dwThreadIdArray = new DWORD[optThreadsNum - 1];
            hThreadArray = new HANDLE[optThreadsNum - 1];
            pThreadParams = new ThreadParams[optThreadsNum];

            // Moved it to a separate loop in order to measure the time of the next loop cleanly
            for (int i = 0; i < optThreadsNum; i++)
            {
                pThreadParams[i].arr = &vec.at((optArrayLength / optThreadsNum) * i);
                pThreadParams[i].number = optNumber;
                // We are not filling further than optThreadsNum-1 'cause the last fragment
                // will be processed with the main thread
                pThreadParams[i].elementsToProcess = optArrayLength / optThreadsNum + (i == optThreadsNum - 1 ? optArrayLength % optThreadsNum : 0);
            }

            auto start_time = Time::now();
            for (int i = 0; i < optThreadsNum - 1; i++)
            {
                hThreadArray[i] = CreateThread(
                    NULL,
                    0,
                    FindElementsGreaterThan,
                    &(pThreadParams[i]),
                    0,
                    &(dwThreadIdArray[i])
                );
            }
            //pThreadParams[optThreadsNum - 1].elementsToProcess = optArrayLength / optThreadsNum + optArrayLength % optThreadsNum;
            FindElementsGreaterThan(&pThreadParams[optThreadsNum - 1]);

            WaitForMultipleObjects(optThreadsNum - 1, hThreadArray, TRUE, INFINITE);

            auto end_time = Time::now();
            fsec fs = end_time - start_time;
            ms d = std::chrono::duration_cast<ms>(fs);

            std::cout << "Multi threaded region has finished in " << d.count() << " ms.\n";

            int result = 0;
            for (int i = 0; i < optThreadsNum; i++)
                result += pThreadParams[i].result;

            std::cout << "Num of elements greater than " <<
                optNumber << " is " << result << std::endl;
        }
        catch (std::string e)
        {
            std::cout << e;
        }
        catch (std::exception& e)
        {
            std::cout << e.what();
        }
    }
    else if (threadType == std_t)
    {
        int* results = new int[optThreadsNum];
        int result = 0;
        std::thread* threads = new std::thread[optThreadsNum];

        int size = optArrayLength / optThreadsNum;

        auto start_time = Time::now();
        for (int i = 0; i < optThreadsNum; i++)
        {
            threads[i] = std::thread(FindElementsGreaterThan_std,
                vec.data() + size * i,
                optNumber,
                // if it is the last thread - add to its array size all extra elements
                size + (i == optThreadsNum-1 ? optArrayLength % optThreadsNum : 0),
                std::ref(results[i]));
        }

        for (int i = 0; i < optThreadsNum; i++)
        {
            threads[i].join();
            result += results[i];
        }

        auto end_time = Time::now();
        fsec fs = end_time - start_time;
        ms d = std::chrono::duration_cast<ms>(fs);

        std::cout << "Multi threaded region has finished in " << d.count() << " ms.\n";

        delete[] threads, results;

        std::cout << "Elements greater than " << optNumber << " is " << result << std::endl;
    }
    else if (threadType == atomic_t)
    {
        atomicResult.store(0);

        int result = 0;
        std::thread* threads = new std::thread[optThreadsNum];

        int size = optArrayLength / optThreadsNum;

        auto start_time = Time::now();
        for (int i = 0; i < optThreadsNum; i++)
        {
            threads[i] = std::thread(FindElementsGreaterThan_atomic,
                                     vec.data() + size * i,
                                     optNumber,
                                     // if it is the last thread - add to its array size all extra elements
                                     size + (i == optThreadsNum - 1 ? optArrayLength % optThreadsNum : 0),
                                     std::ref(atomicResult));
        }

        for (int i = 0; i < optThreadsNum; i++)
        {
            threads[i].join();
        }

        auto end_time = Time::now();
        fsec fs = end_time - start_time;
        ms d = std::chrono::duration_cast<ms>(fs);

        std::cout << "Multi threaded region has finished in " << d.count() << " ms.\n";

        delete[] threads;

        std::cout << "Elements greater than " << optNumber << " is " << atomicResult.load() << std::endl;
    }
    return 0;
}