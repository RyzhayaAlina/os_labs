#include "process_manager.hpp"
#include <iostream>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif
#include <string>

int process_manager(const std::string& cmd) {
    #ifdef _WIN32
        STARTUPINFO si{};
        PROCESS_INFORMATION pi{};
    
        si.cb = sizeof(si);
    
        char cmdBuffer[MAX_PATH];
        strncpy(cmdBuffer, cmd.c_str(), sizeof(cmdBuffer) - 1);
        cmdBuffer[sizeof(cmdBuffer) - 1] = '\0';

        if (!CreateProcess(
                            nullptr,      
                            cmdBuffer,    
                            nullptr,       
                            nullptr, 
                            FALSE,           
                            0,           
                            nullptr,        
                            nullptr,       
                            &si,           
                            &pi            
        )) {
            std::cerr << "CreateProcess failed (" << GetLastError() << ").\n";
            return -1;
        }   

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
            std::cerr << "GetExitCodeProcess failed (" << GetLastError() << ").\n";
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return -1;
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return static_cast<int>(exitCode);
    #else 
        pid_t pid = fork();

        if (pid < 0) {
            std::cerr << "Error: fork() failed. " << strerror(errno) << std::endl;
            return -1;
        } else if (pid == 0) {

            execl("/bin/sh", "/bin/sh", "-c", cmd.c_str(), nullptr);

            std::cerr << "Error: execl() failed. " << strerror(errno) << std::endl;
            _exit(1); 
        }

        int status = 0;

        if (waitpid(pid, &status, 0) == -1) {
            std::cerr << "Error: waitpid() failed. " << strerror(errno) << std::endl;
            return -1;
        }

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            std::cerr << "Error: Process terminated abnormally." << std::endl;
            return -1;
        }
    #endif
}