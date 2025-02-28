#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <clocale>
#include <iomanip>
#include <windows.h>
#include <shellapi.h>
#include <stdexcept>
#include <cstdlib>
#include <algorithm>

namespace fs = std::filesystem;
using namespace std::chrono;

bool copy_file_with_rate(const fs::path &src, const fs::path &dst,
                         double max_speed_mbps = 50.0,
                         size_t buffer_size = 1024 * 1024)
{
    std::ifstream in(src, std::ios::binary);
    std::ofstream out(dst, std::ios::binary);

    if (!in)
        throw std::runtime_error("无法打开源文件: " + src.string());
    if (!out)
        throw std::runtime_error("无法创建目标文件: " + dst.string());

    char *buffer = new char[buffer_size];
    size_t copied = 0;
    const auto start_time = steady_clock::now();
    const double max_speed = max_speed_mbps * 1024 * 1024; // 转换为字节/秒

    try
    {
        while (true)
        {
            const auto chunk_start = steady_clock::now();

            // 读取数据
            in.read(buffer, buffer_size);
            const size_t count = in.gcount();
            if (count == 0)
                break;

            // 写入数据
            out.write(buffer, count);
            if (!out)
            {
                throw std::runtime_error("写入失败，已写入 " + std::to_string(copied) +
                                         " 字节 (错误码: " + std::to_string(GetLastError()) + ")");
            }

            copied += count;

            // 速率控制
            const auto current_time = steady_clock::now();
            const double elapsed = duration<double>(current_time - start_time).count();
            const double expected_time = copied / max_speed;

            if (elapsed < expected_time)
            {
                const DWORD sleep_ms = static_cast<DWORD>((expected_time - elapsed) * 1000);
                Sleep(std::min(sleep_ms, static_cast<DWORD>(1000))); // 最多休眠1秒
            }

            // 调试信息
            const double chunk_duration = duration<double>(steady_clock::now() - chunk_start).count();
            const double speed = (count / (1024.0 * 1024.0)) / std::max(chunk_duration, 0.001);
            std::wcout << L"\r已传输: " << copied / (1024 * 1024) << L"MB "
                       << L"当前速度: " << std::fixed << std::setprecision(1) << speed << L"MB/s";
        }
    }
    catch (...)
    {
        delete[] buffer;
        throw;
    }

    delete[] buffer;
    std::wcout << std::endl;
    return true;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "chs");
    int wargc;
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);

    if (!wargv)
    {
        std::wcerr << L"无法获取命令行参数" << std::endl;
        return 1;
    }

    try
    {
        if (wargc < 3)
        {
            std::wcout << L"用法: " << wargv[0] << L" <源文件> <目标文件> [速率(MB/s] [缓冲区大小(MB)]\n"
                       << L"示例: " << wargv[0] << L" \"C:\\test.bin\" \"D:\\copy.bin\" 100 512\n"
                       << L"默认速率: 50 MB/s\n"
                       << L"默认缓冲区: 1 MB (1048576 bytes)"
                       << std::endl;
            return 1;
        }

        const fs::path src = wargv[1];
        const fs::path dst = wargv[2];
        const double rate = (wargc >= 4) ? std::wcstod(wargv[3], nullptr) : 50.0;
        const size_t buffer = (wargc >= 5) ? static_cast<size_t>(std::wcstoull(wargv[4], nullptr, 10)) * 1024 * 1024 : 1024 * 1024;

        if (rate <= 0 || buffer < 4096)
        {
            throw std::invalid_argument("速率需>0且缓冲区≥4KB");
        }

        std::wcout << L"开始复制: " << src.wstring() << L" -> " << dst.wstring()
                   << L"\n速率限制: " << rate << L" MB/s"
                   << L"\n缓冲区: " << buffer / (1024 * 1024) << L" MB" << std::endl;

        if (copy_file_with_rate(src, dst, rate, buffer))
        {
            std::wcout << L"文件复制成功" << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::wcerr << L"\n错误: " << e.what() << std::endl;
        LocalFree(wargv);
        return 2;
    }

    LocalFree(wargv);
    return 0;
}