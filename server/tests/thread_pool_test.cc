#include "thread_pool.h"

#include <atomic>
#include <iostream>

int main(){
    // 创建线程池
    smart_home::ThreadPool pool(4, 1000);
    std::atomic<int> counter(0);

    for(int i = 0; i < 1000; i++){
        pool.addTask([&counter] (){
            counter.fetch_add(1);
        });
    }

    pool.stop();
    if(counter.load() == 1000){
        std::cout << "ThreadPool test passed." << std::endl;
        return 0;
    } else {
        std::cout << "ThreadPool test failed: " << counter.load() << std::endl;
        return 1;
    }
}