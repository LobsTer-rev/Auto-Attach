# Auto-Attach
Lightweight C++ utility for monitoring, restarting and attaching to a process, useful for injectors or game hacking tools.

### Features
Auto-launch a process if it's not running
Auto-restart support when the target exits
Attach to the target process and retrieve its process handle and main window handle
Background monitoring using a dedicated thread

### Dependencies
Windows API (Windows.h, TlHelp32.h)
C++17

### Example Usage
```
#include "AutoAttach.h"

int main() {
    AutoAttach attach("example.exe", "C:\\Path\\To\\example.exe", true);

    // Start monitoring the process in a background thread
    attach.Start();

    // Access process info such as handle and hwnd
    ProcessInfo* info = attach.getProcessInfo();

    // Monitor status
    while (true) {
        std::cout << "Process status: " << info->status << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  
    return 0;
}
```
This example will:
- Monitor `example.exe`
- Launch it if it's not running
- Attempt to attach and retrieve a handle
- Auto-restart the process if it exits

### License

[MIT](LICENSE) — free for personal and commercial use.
