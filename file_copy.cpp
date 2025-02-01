#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#ifdef _WIN32
#include <windows.h>
#endif

// 初始化控制台
void init_console()
{
#ifdef _WIN32
    // 启用 ANSI 转义序列
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    // 设置控制台输出为 UTF-8
    SetConsoleOutputCP(CP_UTF8);
#endif
}

// 显示进度条
void show_progress(double percentage, std::streamsize bytes_copied, std::streamsize total_bytes)
{
    const int bar_width = 50;
    int pos = bar_width * percentage;

    // 清除当前行
    std::cout << "\033[2K\r";

    // 显示进度条
    std::cout << "[";
    for (int i = 0; i < bar_width; ++i)
    {
        if (i < pos)
            std::cout << "=";
        else if (i == pos)
            std::cout << ">";
        else
            std::cout << " ";
    }

    // 显示百分比和已复制大小/总大小
    std::cout << "] " << std::fixed << std::setprecision(1) << percentage * 100.0 << "% "
              << std::fixed << std::setprecision(2)
              << (bytes_copied / 1024.0 / 1024.0) << "MB/"
              << (total_bytes / 1024.0 / 1024.0) << "MB    ";
    std::cout.flush();
}

int main(int argc, char *argv[])
{
    // 初始化控制台
    init_console();

    bool silent_mode = false;
    int source_index = 1;
    double max_speed = -1; // MB/s，-1表示不限速

    // 处理参数
    while (source_index < argc && argv[source_index][0] == '-')
    {
        std::string arg = argv[source_index];
        if (arg == "-x")
        {
            silent_mode = true;
            source_index++;
        }
        else if (arg == "-M" && source_index + 1 < argc)
        {
            max_speed = std::stod(argv[source_index + 1]);
            source_index += 2;
        }
        else
        {
            if (!silent_mode)
            {
                std::cerr << "无效的参数: " << arg << std::endl;
            }
            return 1;
        }
    }

    // 检查参数数量是否正确
    if (argc != source_index + 2)
    {
        if (!silent_mode)
        {
            std::cerr << "使用方法: " << argv[0] << " [-x 静默] [-M 速度] <源文件路径> <目标文件路径>" << std::endl;
            std::cerr << "选项:" << std::endl;
            std::cerr << "  -x     静默模式，不输出任何提示信息" << std::endl;
            std::cerr << "  -M <n> 限制最大传输速度为n MB/s" << std::endl;
        }
        return 1;
    }

    std::string source_path = argv[source_index];
    std::string dest_path = argv[source_index + 1];

    // 以二进制模式打开源文件
    std::ifstream source(source_path, std::ios::binary);
    if (!source)
    {
        if (!silent_mode)
        {
            std::cerr << "无法打开源文件: " << source_path << std::endl;
        }
        return 1;
    }

    // 获取文件大小
    source.seekg(0, std::ios::end);
    std::streamsize total_size = source.tellg();
    source.seekg(0, std::ios::beg);

    // 以二进制模式打开目标文件
    std::ofstream dest(dest_path, std::ios::binary);
    if (!dest)
    {
        if (!silent_mode)
        {
            std::cerr << "无法创建目标文件: " << dest_path << std::endl;
        }
        return 1;
    }

    // 带速度限制的复制
    const int BUFFER_SIZE = 1024 * 1024; // 1MB的缓冲区
    char *buffer = new char[BUFFER_SIZE];
    std::streamsize total_bytes_copied = 0;

    try
    {
        while (source)
        {
            auto start_time = std::chrono::steady_clock::now();

            // 读取并写入数据
            source.read(buffer, BUFFER_SIZE);
            std::streamsize bytes_read = source.gcount();
            if (bytes_read > 0)
            {
                dest.write(buffer, bytes_read);
                total_bytes_copied += bytes_read;

                // 显示进度条（非静默模式下）
                if (!silent_mode)
                {
                    double progress = static_cast<double>(total_bytes_copied) / total_size;
                    show_progress(progress, total_bytes_copied, total_size);
                }

                // 如果设置了速度限制，控制读写速度
                if (max_speed > 0)
                {
                    auto end_time = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

                    // 计算理想时间（微秒）
                    double ideal_time = (bytes_read / (max_speed * 1024 * 1024)) * 1000000;

                    // 如果实际用时小于理想时间，则等待
                    if (duration.count() < ideal_time)
                    {
                        std::this_thread::sleep_for(std::chrono::microseconds(
                            static_cast<int>(ideal_time - duration.count())));
                    }
                }
            }
        }
    }
    catch (...)
    {
        delete[] buffer;
        if (!silent_mode)
        {
            std::cout << std::endl; // 换行，避免进度条残留
            std::cerr << "复制过程中发生错误" << std::endl;
        }
        return 1;
    }

    delete[] buffer;

    // 检查复制是否成功
    if (!dest)
    {
        if (!silent_mode)
        {
            std::cout << std::endl; // 换行，避免进度条残留
            std::cerr << "复制文件时发生错误" << std::endl;
        }
        return 1;
    }

    if (!silent_mode)
    {
        std::cout << std::endl; // 换行，结束进度条
        std::cout << "文件复制成功！" << std::endl;
    }
    return 0;
}