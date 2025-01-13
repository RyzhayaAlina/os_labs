#include <iostream>
#include <string>
#include <fstream>
#include <ctime>
#include <thread>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <io.h> // Для использования _access
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <csignal>
#include <sys/wait.h>
#endif

std::mutex log_mutex, counter_mutex;  // Мьютекс для логирования и обновления counter в POSIX

#ifdef _WIN32
HANDLE counter_mutex_win;  // Мьютекс для Windows
HANDLE shared_memory;
volatile int* counter;
typedef DWORD pid_t;
#else
int* counter;
#endif

bool is_master = true;
std::string marker_file = "master_marker.txt"; // Файл-маркер для определения первого запуска

// Функция для логирования сообщений
void log(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::ofstream log_file("process_log.txt", std::ios::app);
    if (log_file.is_open()) {
        time_t now = time(0);
#ifdef _WIN32
        tm local_time;
        localtime_s(&local_time, &now);
#else
        tm* local_time = localtime(&now);
#endif
        log_file << 1900 + local_time.tm_year << "-"
                 << 1 + local_time.tm_mon << "-"
                 << local_time.tm_mday << " "
                 << local_time.tm_hour << ":"
                 << local_time.tm_min << ":"
                 << local_time.tm_sec << " "
                 << message << std::endl;
        log_file.close();
    }
}

// Функция для инкремента переменной counter
void increment_counter() {
    while (true) {
#ifdef _WIN32
        WaitForSingleObject(counter_mutex_win, INFINITE);
        (*counter)++;
        ReleaseMutex(counter_mutex_win);
        Sleep(300);  // Используем Sleep в Windows (в миллисекундах)
#else
        usleep(300000);  // В POSIX системах используется usleep (в микросекундах)
        __sync_fetch_and_add(counter, 1);
#endif
    }
}

// Функция для вывода текущего состояния
void log_current_state(pid_t pid) {
    while (is_master) {
#ifdef _WIN32
        Sleep(1000);  // Используем Sleep в Windows (в миллисекундах)
#else
        sleep(1);  // В POSIX системах используется sleep (в секундах)
#endif
#ifdef _WIN32
        WaitForSingleObject(counter_mutex_win, INFINITE);
#else
        // Mutex equivalent for POSIX could be added if needed
#endif
        log("PID: " + std::to_string(pid) + " Counter: " + std::to_string(*counter));
#ifdef _WIN32
        ReleaseMutex(counter_mutex_win);
#endif
    }
}

// Функция для изменения переменной counter на определенную величину для Windows
#ifdef _WIN32
void process_function_win(const std::string& task, pid_t parent_pid) {
    if (task == "increment_by_10") {
        log("Child PID: " + std::to_string(GetCurrentProcessId()) + " incremented counter by 10 and exiting.");
        (*counter) += 10;
    } else if (task == "double_and_restore") {
        log("Child PID: " + std::to_string(GetCurrentProcessId()) + " doubled counter and waiting to restore.");
        (*counter) *= 2;
        Sleep(2000);  // Задержка для имитации ожидания
        (*counter) /= 2;
    } else {
        log("Unknown task for child process.");
    }
    exit(0);
}
#endif

// Функция для изменения переменной counter на определенную величину для POSIX
#ifndef _WIN32
void process_function_posix(const std::string& task, pid_t parent_pid) {
    if (task == "increment_by_10") {
        log("Child PID: " + std::to_string(getpid()) + " incremented counter by 10 and exiting.");
        __sync_fetch_and_add(counter, 10);
    } else if (task == "double_and_restore") {
        log("Child PID: " + std::to_string(getpid()) + " doubled counter and waiting to restore.");
        __sync_fetch_and_mul(counter, 2);
        sleep(2);  // Задержка для имитации ожидания
        __sync_fetch_and_div(counter, 2);
    } else {
        log("Unknown task for child process.");
    }
    exit(0);
}
#endif

// Функция для вызова соответствующей версии process_function
void process_function(const std::string& task, pid_t parent_pid) {
#ifdef _WIN32
    process_function_win(task, parent_pid);
#else
    process_function_posix(task, parent_pid);
#endif
}

// Функция для запуска нового процесса с заданной задачей
void spawn_process(pid_t parent_pid, const std::string& task) {
#ifdef _WIN32
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    std::string command_line = std::string(GetCommandLineA()) + " --task " + task;

    int buffer_size = MultiByteToWideChar(CP_UTF8, 0, command_line.c_str(), -1, NULL, 0);
    wchar_t* w_command_line = new wchar_t[buffer_size];
    MultiByteToWideChar(CP_UTF8, 0, command_line.c_str(), -1, w_command_line, buffer_size);

    if (CreateProcess(NULL, w_command_line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        log("Child PID: " + std::to_string(pi.dwProcessId) + " spawned by Parent PID: " + std::to_string(parent_pid));
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        log("Failed to spawn child process.");
    }

    delete[] w_command_line;
#else
    pid_t pid = fork();
    if (pid == 0) {
        process_function(task, parent_pid);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
#endif
}

// Главная функция, которая запускает процесс
void master_process(pid_t pid) {
    // Если файл маркер существует, это не первый процесс
#ifdef _WIN32
    if (_access(marker_file.c_str(), 0) != -1) {
#else
    if (access(marker_file.c_str(), F_OK) != -1) {
#endif
        is_master = false;
    } else {
        // Если это первый процесс, создаем маркерный файл
        std::ofstream file(marker_file);
        file.close();
    }

#ifdef _WIN32
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)log_current_state, (LPVOID)pid, 0, NULL);
#else
    std::thread logger_thread(log_current_state, pid);
    logger_thread.detach();
#endif

    while (true) {
#ifdef _WIN32
        Sleep(3000);  // Используем Sleep в Windows (в миллисекундах)
#else
        sleep(3);  // В POSIX системах используется sleep (в секундах)
#endif
        if (is_master) {
            log("Spawn new process");
            spawn_process(pid, "increment_by_10");
            spawn_process(pid, "double_and_restore");
        }
    }
}

// Функция для обработки ввода числа
void input_counter_value() {
    std::string input;
    while (true) {
        std::cout << "Enter a number to set the counter: ";
        std::getline(std::cin, input);
        try {
            int value = std::stoi(input);
            std::lock_guard<std::mutex> lock(counter_mutex); // Защищаем доступ к counter
#ifdef _WIN32
            WaitForSingleObject(counter_mutex_win, INFINITE);
            *counter = value;
            ReleaseMutex(counter_mutex_win);
#else
            *counter = value;
#endif
            log("Counter set to: " + std::to_string(value));
        } catch (const std::invalid_argument& e) {
            std::cout << "Invalid input, please enter a valid number." << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    counter_mutex_win = CreateMutex(NULL, FALSE, NULL);
    shared_memory = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(int), L"Global\\SharedCounter");
    counter = (volatile int*)MapViewOfFile(shared_memory, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(int));
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        *counter = 0;
    }
#else
    counter = (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *counter = 0;
#endif

#ifdef _WIN32
    pid_t pid = GetCurrentProcessId();
#else
    pid_t pid = _getpid();
#endif

    log("Process started with PID: " + std::to_string(pid));

    if (argc > 2 && std::string(argv[1]) == "--task") {
        is_master = false;
        log("process_function");
        process_function(argv[2], pid);
    } else {
#ifdef _WIN32
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)increment_counter, NULL, 0, NULL);
#else
        std::thread increment_thread(increment_counter);
        increment_thread.detach();
#endif

        // Запуск потока для ввода числа
        std::thread input_thread(input_counter_value);
        input_thread.detach();

        master_process(pid);
    }

#ifdef _WIN32
    UnmapViewOfFile((LPCVOID)counter);
    CloseHandle(shared_memory);
    CloseHandle(counter_mutex_win);
#endif

    return 0;
}
