#include "AutoAttach.h"
#include <iostream>
#include <tlhelp32.h>
#include <thread>
#include <system_error>

AutoAttach::AutoAttach(const std::string& processName) {
    pInfo.processName = processName;
    pInfo.status = NOT_RUNNING;
}

AutoAttach::AutoAttach(const std::string& processName, const std::string& processPath, bool restart)
    : autoRestart(restart) {
    pInfo.processName = processName;
    pInfo.fullPath = processPath;
    pInfo.status = NOT_RUNNING;
}

AutoAttach::~AutoAttach() {
    Stop();
    if (pInfo.hProc) {
        CloseHandle(pInfo.hProc);
        pInfo.hProc = nullptr;
    }
}

bool AutoAttach::LaunchProcess(const std::filesystem::path& processPath, std::error_code& ec) {
    if (!std::filesystem::exists(processPath)) {
        ec = std::make_error_code(std::errc::no_such_file_or_directory);
        LogError("LaunchProcess - file not found", ec);
        return false;
    }

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (!CreateProcessA(
        processPath.string().c_str(),
        nullptr, nullptr, nullptr,
        FALSE, 0, nullptr, nullptr,
        &si, &pi)) {
        ec = std::error_code(GetLastError(), std::system_category());
        LogError("LaunchProcess - CreateProcessA failed", ec);
        return false;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

bool AutoAttach::LaunchProcess() {
    std::error_code ec;
    return LaunchProcess(pInfo.fullPath, ec);
}

bool AutoAttach::Attach(std::error_code& ec) {
    pInfo.pid = FindProcessIdByName();
    if (pInfo.pid <= 0) {
        ec = std::make_error_code(std::errc::no_such_process);
        LogError("Attach - PID not found", ec);
        return false;
    }

    pInfo.hwnd = FindMainWindow();
    if (!pInfo.hwnd) {
        ec = std::make_error_code(std::errc::no_such_device);
        LogError("Attach - Main window not found", ec);
    }

    pInfo.hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pInfo.pid);
    if (!pInfo.hProc) {
        ec = std::error_code(GetLastError(), std::system_category());
        LogError("Attach - OpenProcess failed", ec);
        return false;
    }

    pInfo.status = ATTACHED;
    return true;
}

bool AutoAttach::Attach() {
    std::error_code ec;
    return Attach(ec);
}

void AutoAttach::Stop() {
    isMonitoring = false;
    if (monitorThread && monitorThread->joinable()) {
        monitorThread->join();
    }
    monitorThread.reset();

    if (pInfo.hProc) {
        CloseHandle(pInfo.hProc);
        pInfo.hProc = nullptr;
    }
    pInfo.status = NOT_RUNNING;
}

bool AutoAttach::KillProcess() {
    if (!pInfo.hProc)
        return false;
    bool result = TerminateProcess(pInfo.hProc, 0);
    Stop();
    return result;
}

BOOL CALLBACK AutoAttach::EnumWindowsProc(HWND hWnd, LPARAM lParam) {
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hWnd, &windowPid);
    auto* info = reinterpret_cast<ProcessInfo*>(lParam);
    if (info && static_cast<DWORD>(info->pid) == windowPid) {
        info->hwnd = hWnd;
        return FALSE;
    }
    return TRUE;
}

int AutoAttach::FindProcessIdByName() const {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return -1;

    PROCESSENTRY32 pe32 = { sizeof(pe32) };
    std::wstring targetName(pInfo.processName.begin(), pInfo.processName.end());

    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (wcscmp(pe32.szExeFile, targetName.c_str()) == 0) {
                CloseHandle(hSnapshot);
                return pe32.th32ProcessID;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return -1;
}

HWND AutoAttach::FindMainWindow() {
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&pInfo));
    return pInfo.hwnd;
}

void AutoAttach::MonitorProcess(std::error_code& ec) {
    while (isMonitoring) {
        ec.clear();
        pInfo.pid = FindProcessIdByName();

        if (pInfo.pid <= 0) {
            pInfo.status = NOT_RUNNING;

            if (autoRestart) {
                pInfo.status = RESTARTING;
                LaunchProcess(pInfo.fullPath, ec);
            }
        }
        else {
            if (!Attach(ec)) {
                pInfo.status = RUNNING; // fallback state
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

void AutoAttach::MonitorProcess() {
    std::error_code ec;
    MonitorProcess(ec);
}

void AutoAttach::Start() {
    if (isMonitoring) return;
    isMonitoring = true;
    monitorThread = std::make_unique<std::thread>([this]() {
        this->MonitorProcess();
        });
}

void AutoAttach::Start(std::error_code& ec) {
    if (isMonitoring) return;
    isMonitoring = true;
    monitorThread = std::make_unique<std::thread>([this,&ec]() {
        this->MonitorProcess(ec);
        });
}

ProcessInfo* AutoAttach::getProcessInfo() {
    return &pInfo;
}

void AutoAttach::LogError(const std::string& context, const std::error_code& ec) const {
    std::cerr << "[Error] " << context << ": " << ec.message()
        << " (code: " << ec.value() << ")" << std::endl;
}
