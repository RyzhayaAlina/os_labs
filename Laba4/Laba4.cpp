#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <deque>
#include <fstream>
#include <numeric>
#include <iomanip>
#include <sstream>
#include <random>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#endif

// Helper function to get the current timestamp as a string
std::string getCurrentTimestamp() {
#ifdef _WIN32
    SYSTEMTIME time;
    GetLocalTime(&time);
    std::stringstream ss;
    ss << time.wYear << "-" << std::setw(2) << std::setfill('0') << time.wMonth << "-"
       << std::setw(2) << std::setfill('0') << time.wDay << " "
       << std::setw(2) << std::setfill('0') << time.wHour << ":"
       << std::setw(2) << std::setfill('0') << time.wMinute << ":"
       << std::setw(2) << std::setfill('0') << time.wSecond;
    return ss.str();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *localTime = localtime(&tv.tv_sec);
    std::stringstream ss;
    ss << (localTime->tm_year + 1900) << "-" << std::setw(2) << std::setfill('0') << (localTime->tm_mon + 1) << "-"
       << std::setw(2) << std::setfill('0') << localTime->tm_mday << " "
       << std::setw(2) << std::setfill('0') << localTime->tm_hour << ":"
       << std::setw(2) << std::setfill('0') << localTime->tm_min << ":"
       << std::setw(2) << std::setfill('0') << localTime->tm_sec;
    return ss.str();
#endif
}

time_t systemTimeToTimeT(const SYSTEMTIME &systemTime) {
    struct tm timeInfo;
    timeInfo.tm_year = systemTime.wYear - 1900;
    timeInfo.tm_mon = systemTime.wMonth - 1;
    timeInfo.tm_mday = systemTime.wDay;
    timeInfo.tm_hour = systemTime.wHour;
    timeInfo.tm_min = systemTime.wMinute;
    timeInfo.tm_sec = systemTime.wSecond;
    timeInfo.tm_isdst = -1;  // Unknown daylight saving time status

    return mktime(&timeInfo);
}

// Helper function to configure the serial port
#ifdef _WIN32
HANDLE configureSerialPort(const std::string &portName) {
    std::wstring wPortName(portName.begin(), portName.end());
    HANDLE hSerial = CreateFile(wPortName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening serial port" << std::endl;
        return INVALID_HANDLE_VALUE;
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error getting serial port state" << std::endl;
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    dcbSerialParams.BaudRate = CBR_9600;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error setting serial port state" << std::endl;
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    return hSerial;
}
#else
int configureSerialPort(const std::string &portName) {
    int fd = open(portName.c_str(), O_RDWR | O_NOCTTY);
    if (fd == -1) {
        std::cerr << "Error opening serial port" << std::endl;
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "Error getting serial port attributes" << std::endl;
        close(fd);
        return -1;
    }

    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
    tty.c_iflag &= ~IGNBRK;                     // disable break processing
    tty.c_lflag = 0;                            // no signaling chars, no echo, no canonical processing
    tty.c_oflag = 0;                            // no remapping, no delays
    tty.c_cc[VMIN] = 1;                         // read doesn't block
    tty.c_cc[VTIME] = 1;                        // 0.1 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl
    tty.c_cflag |= (CLOCAL | CREAD);       // ignore modem controls, enable reading
    tty.c_cflag &= ~(PARENB | PARODD);     // shut off parity
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "Error setting serial port attributes" << std::endl;
        close(fd);
        return -1;
    }

    return fd;
}
#endif

// Function to simulate device writing temperature data to the serial port
void simulateDevice(const std::string &portName) {
#ifdef _WIN32
    HANDLE hSerial = configureSerialPort(portName);
    if (hSerial == INVALID_HANDLE_VALUE) return;
#else
    int fd = configureSerialPort(portName);
    if (fd == -1) return;
#endif

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dist(-20.0, 40.0);

    while (true) {
        double simulatedTemperature = dist(gen);
        std::stringstream ss;
        ss << simulatedTemperature << "\n";

#ifdef _WIN32
        DWORD bytesWritten;
        WriteFile(hSerial, ss.str().c_str(), ss.str().length(), &bytesWritten, NULL);
#else
        write(fd, ss.str().c_str(), ss.str().length());
#endif

#ifdef _WIN32
        Sleep(10000);
#else
        sleep(10);
#endif
    }

#ifdef _WIN32
    CloseHandle(hSerial);
#else
    close(fd);
#endif
}

// Function to read from the serial port
std::string readFromSerialPort(const std::string &portName) {
#ifdef _WIN32
    HANDLE hSerial = configureSerialPort(portName);
    if (hSerial == INVALID_HANDLE_VALUE) return "";

    char buffer[256];
    DWORD bytesRead;
    if (!ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
        std::cerr << "Error reading from serial port" << std::endl;
        CloseHandle(hSerial);
        return "";
    }
    buffer[bytesRead] = '\0';
    CloseHandle(hSerial);
    return std::string(buffer);
#else
    int fd = configureSerialPort(portName);
    if (fd == -1) return "";

    char buffer[256];
    int bytesRead = read(fd, buffer, sizeof(buffer) - 1);
    if (bytesRead < 0) {
        std::cerr << "Error reading from serial port" << std::endl;
        close(fd);
        return "";
    }
    buffer[bytesRead] = '\0';
    close(fd);
    return std::string(buffer);
#endif
}

int main() {
    const std::string allMeasurementsLog = "all_measurements.log";
    const std::string hourlyAveragesLog = "hourly_averages.log";
    const std::string dailyAveragesLog = "daily_averages.log";

    const std::string portName =
#ifdef _WIN32
        R"(\\.\COM1)";
#else
        "/dev/ttyS0";
#endif

    std::deque<std::pair<std::time_t, double>> measurements;
    std::vector<double> hourlyTemperatures;
    std::vector<double> dailyTemperatures;

#ifdef _WIN32
    SYSTEMTIME lastHour, lastDay;
    GetLocalTime(&lastHour);
    lastDay = lastHour;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *localTime = localtime(&tv.tv_sec);
    struct tm lastHour = *localTime;
    struct tm lastDay = lastHour;
#endif

    // Start the simulated device in a separate thread
    std::thread deviceThread(simulateDevice, portName);
    deviceThread.detach();

    while (true) {
        std::string data = readFromSerialPort(portName);
        if (data.empty()) {
#ifdef _WIN32
            Sleep(10000);
#else
            sleep(10);
#endif
            continue;
        }

        try {
            double temperature = std::stod(data);

#ifdef _WIN32
            SYSTEMTIME now;
            GetLocalTime(&now);
            time_t currentTime = systemTimeToTimeT(now);
#else
            gettimeofday(&tv, NULL);
            time_t currentTime = tv.tv_sec;
            struct tm *now = localtime(&currentTime);
#endif
            measurements.emplace_back(currentTime, temperature);

            // Remove old measurements (older than 24 hours)
            while (!measurements.empty() && (currentTime - measurements.front().first) >= 86400) {
                measurements.pop_front();
            }

            // Write only recent measurements to the log file
            std::ofstream allLog(allMeasurementsLog, std::ios::trunc);
            for (const auto &entry : measurements) {
                std::time_t timestamp = entry.first;
                struct tm timeInfo;
#ifdef _WIN32
                localtime_s(&timeInfo, &timestamp);
#else
                localtime_r(&timestamp, &timeInfo);
#endif
                char timeBuffer[20];
                strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
                allLog << timeBuffer << ", " << entry.second << "\n";
            }

#ifdef _WIN32
            // Check if an hour has passed
            if ((now.wHour != lastHour.wHour) || (now.wDay != lastHour.wDay)) {
                lastHour = now;
#else
            // Check if an hour has passed
            if ((now->tm_hour != lastHour.tm_hour) || (now->tm_mday != lastHour.tm_mday)) {
                lastHour = *now;
#endif
                if (!hourlyTemperatures.empty()) {
                    double hourlyAverage = std::accumulate(hourlyTemperatures.begin(), hourlyTemperatures.end(), 0.0) / hourlyTemperatures.size();
                    std::ofstream hourlyLog(hourlyAveragesLog, std::ios::app);
                    hourlyLog << getCurrentTimestamp() << ", " << hourlyAverage << "\n";
                    dailyTemperatures.push_back(hourlyAverage);
                    hourlyTemperatures.clear();
                }
            }

#ifdef _WIN32
            // Check if a day has passed
            if (now.wDay != lastDay.wDay) {
                lastDay = now;
#else
            // Check if a day has passed
            if (now->tm_mday != lastDay.tm_mday) {
                lastDay = *now;
#endif
                if (!dailyTemperatures.empty()) {
                    double dailyAverage = std::accumulate(dailyTemperatures.begin(), dailyTemperatures.end(), 0.0) / dailyTemperatures.size();
                    std::ofstream dailyLog(dailyAveragesLog, std::ios::app);
                    dailyLog << getCurrentTimestamp() << ", " << dailyAverage << "\n";
                    dailyTemperatures.clear();
                }
            }

            hourlyTemperatures.push_back(temperature);
        } catch (const std::exception &e) {
            std::cerr << "Error parsing temperature data: " << e.what() << std::endl;
        }

#ifdef _WIN32
        Sleep(10000);
#else
        sleep(10);
#endif
    }

    return 0;
}