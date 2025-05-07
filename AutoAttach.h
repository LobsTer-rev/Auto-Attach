#pragma once
#include <string>
#include <filesystem>
#include <optional>
#include <system_error>
#include <thread>
#include <atomic>
#include <windows.h>

enum ProcessStatus {
    NOT_RUNNING,
    STARTING,
    RUNNING,
    RESTARTING,
    ATTACHED
};

struct ProcessInfo {
    std::string processName;
    std::filesystem::path fullPath;
    int pid = -1;
    HWND hwnd = nullptr;
    HANDLE hProc = nullptr;
    ProcessStatus status = NOT_RUNNING;
};

class AutoAttach {
public:
    AutoAttach(const std::string& processName);
    AutoAttach(const std::string& processName, const std::string& processPath, bool restart);
    ~AutoAttach();

    bool LaunchProcess(const std::filesystem::path& path, std::error_code& ec);
    bool LaunchProcess();
    bool Attach(std::error_code& ec);
    bool Attach();
    bool KillProcess();
    void Stop();
    void Start();
    void Start(std::error_code& ec);

    ProcessInfo* getProcessInfo();

private:
    static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam);
    int FindProcessIdByName() const;
    HWND FindMainWindow();
    void MonitorProcess(std::error_code& ec);
    void MonitorProcess();

    void LogError(const std::string& context, const std::error_code& ec) const;

private:
    ProcessInfo pInfo;
    std::atomic<bool> isMonitoring = false;
    bool autoRestart = false;
    std::unique_ptr<std::thread> monitorThread;
};
