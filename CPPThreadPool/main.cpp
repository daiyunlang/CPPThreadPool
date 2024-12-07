#include <cstdio>
#include "ThreadPool.h"
#include<chrono>
void Func(int num) {
    printf("this thread id : %d, num : %d", std::this_thread::get_id(), num);
}
int main()
{
    ThreadPool tPool(5, 2);
    for (int i = 0; i < 10; ++i) {
        int num = i + 1;
        tPool.EnQueue(Func, num);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}