//第一步：验证工具链 + 建立"断言式"测试骨架
//
//只做两件事：
//1.确认Qt自带的MinGw g++能编译C++11代码（后面媒体代码要用C++11特性）
//2.建立“断言式”测试习惯：每个测试程序 = 若干条 expect（条件，说明），
//  最后返回0（全部通过）或1（有失败），方便自动判断成败

#include <iostream> //std::cout/std::endl: 打印测试结果

//失败计数：全局变量，统计有多少条断言没通过
//static 让这个变量只在当前文件可见，避免和别的文件起冲突
static int g_failures = 0;

//断言工具函数：
// cond 为true -> 打印[ok]
// cond 为false -> 打印[FAIL] 并让失败计数 +1
//参数用const char* 传说明文件，避免拷贝字符串的开销
static void expect(bool cond,const char *msg) {
    if(cond) {
        std::cout << " [ok] " << msg << std::endl;
    } else {
        std::cout << " [FAIL] " << msg << std::endl;
        ++g_failures;
    }
}

int main() {
    std::cout << "=== 第一步测试开始 ===" << std::endl;

    // --- 验证C++11特性可用 (后面写媒体代码都会用到)
    auto answer = 40 + 2;//auto: 让编译器自动推导类型（这里推导为 int )
    int *p = nullptr;  // nullptr:C++11的空指针，比旧的NULL更安全
    expect(answer == 42,"auto类型推导正常");
    expect(p == nullptr,"nullptr可用");

    //---汇总---
    std::cout << "=== 第一步测试结束 ===" << std::endl;
    if(g_failures == 0) {
        std::cout << "test_toolchain passed." << std::endl;
        return 0;     // 0 = 测试通过
    }
    std::cout << "test_toolchain FAILED:" << g_failures << "项未通过。" << std::endl;
    return 1;         //非0 = 测试失败
}