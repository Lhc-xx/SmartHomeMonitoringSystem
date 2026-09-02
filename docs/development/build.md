# CMake 说明

## 推荐命令

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 常用开关

- `-DBUILD_SERVER=ON|OFF`
- `-DBUILD_LINUX_CLIENT=ON|OFF`
- `-DBUILD_QT_CLIENT=ON|OFF`
- `-DBUILD_TESTS=ON|OFF`

Qt 客户端默认关闭，待安装 Qt 5.14.2 后再打开，并在 `client/qt/CMakeLists.txt` 中补充 Qt 工具链与业务源码。
