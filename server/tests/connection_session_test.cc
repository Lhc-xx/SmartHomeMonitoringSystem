// connection_session_test.cc —— 连接级登录状态机与会话超时（角色 A）
//
// 只验证 Connection 上的登录态，不依赖 MySQL / log4cpp / epoll：
//   markLoggedIn / markLoggedOut / isAuthenticated / userId / token /
//   loginTime / isSessionExpired
#include "connection.h"

#include <cstdio>
#include <string>

using smart_home::Connection;

static int g_failures = 0;
static void expect(bool cond, const char *msg) {
    if (cond) {
        std::printf("  [ok]   %s\n", msg);
    } else {
        std::printf("  [FAIL] %s\n", msg);
        ++g_failures;
    }
}

int main() {
    std::printf("=== Connection session state test begin ===\n");

    // fd 传 -1：本测试不触发任何 socket 读写，-1 保证析构不会误 close。
    Connection conn(-1);

    // 1) 初始未登录
    expect(!conn.isAuthenticated(), "initially not authenticated");
    expect(conn.userId() == 0U, "userId() == 0 initially");
    expect(conn.token().empty(), "token empty initially");
    expect(conn.loginTime() == 0, "loginTime() == 0 initially");

    // 2) 未登录时 isSessionExpired 恒为 false
    expect(!conn.isSessionExpired(0, 1), "unauthenticated never expired");
    expect(!conn.isSessionExpired(99999, 1), "unauthenticated never expired (future)");

    // 3) 登录
    conn.markLoggedIn(42U, "test-token");
    expect(conn.isAuthenticated(), "authenticated after login");
    expect(conn.userId() == 42U, "userId() == 42");
    expect(conn.token() == "test-token", "token stored");
    expect(conn.loginTime() > 0, "loginTime set after login");

    const time_t loginAt = conn.loginTime();

    // 4) 会话超时边界（now - loginTime >= timeoutSeconds 视为超时）
    expect(!conn.isSessionExpired(loginAt + 1799, 1800), "not expired at 1799s");
    expect(conn.isSessionExpired(loginAt + 1800, 1800), "expired at exactly 1800s");
    expect(conn.isSessionExpired(loginAt + 99999, 1800), "expired far in future");

    // 5) 超时时间 <= 0 视为永不超时
    expect(!conn.isSessionExpired(loginAt + 99999, 0), "timeout 0 means never expire");
    expect(!conn.isSessionExpired(loginAt + 99999, -1), "negative timeout means never expire");

    // 6) 登出
    conn.markLoggedOut();
    expect(!conn.isAuthenticated(), "not authenticated after logout");
    expect(conn.userId() == 0U, "userId() == 0 after logout");
    expect(conn.token().empty(), "token empty after logout");
    expect(conn.loginTime() == 0, "loginTime() == 0 after logout");
    expect(!conn.isSessionExpired(loginAt + 99999, 1), "unauthenticated never expired again");

    std::printf("=== Connection session state test end ===\n");
    if (g_failures == 0) {
        std::printf("connection_session test passed.\n");
        return 0;
    }
    std::printf("connection_session test FAILED: %d items.\n", g_failures);
    return 1;
}
