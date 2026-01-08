# Fixes Needed for xpuDaemon Build

## DaemonController.cpp - Platform Type Issues

**File**: `C:\workspace\cliMusic\xpu\src\xpuDaemon\DaemonController.cpp`

### Issue 1: Missing `process_id_t` typedef and `pid_t` usage

**Fix**: Add platform-specific typedefs at the top of the file (after line 20):

```cpp
#ifdef PLATFORM_LINUX
#include <sys/types.h>
typedef pid_t process_id_t;
#elif defined(PLATFORM_WINDOWS)
typedef DWORD process_id_t;
#elif defined(PLATFORM_MACOS)
#include <sys/types.h>
typedef pid_t process_id_t;
#endif
```

**Then replace all `pid_t` with `process_id_t`:**
- Line 34: `pid_t pid;` → `process_id_t pid;`
- Line 124: `pid_t pid = fork();` → `process_id_t pid = fork();`
- Line 136: `pid_t sid = setsid();` → `process_id_t sid = setsid();`
- Line 211: `pid_t pid;` → `process_id_t pid;`

### Issue 2: Missing `FileDeleteError` error code

**File**: `C:\workspace\cliMusic\xpu\src\lib\protocol\ErrorCode.h`

**Fix**: Add to File errors section (around line 65-74):
```cpp
FileDeleteError = XX,  // Add appropriate number
```

Add to toString function:
```cpp
case ErrorCode::FileDeleteError: return "FileDeleteError";
```

### Issue 3: Deprecated `getpid` on Windows

**File**: `C:\workspace\cliMusic\xpu\src\xpuDaemon\DaemonController.cpp` line 188

**Fix**: Use platform-specific function:
```cpp
#ifdef PLATFORM_WINDOWS
    process_id_t pid = GetCurrentProcessId();
#else
    process_id_t pid = getpid();
#endif
```

## ProcessManager.cpp - Windows Compatibility

**File**: `C:\workspace\cliMusic\xpu\src\xpuDaemon\ProcessManager.cpp`

### Issue: Missing `sys/wait.h` on Windows

**Fix**: Wrap in platform guard:
```cpp
#ifdef PLATFORM_LINUX
#include <sys/wait.h>
#endif
```

Also need to handle `waitpid()` and related POSIX functions that don't exist on Windows.

## xpuDaemon.cpp - Method Signature Mismatches

**File**: `C:\workspace\cliMusic\xpu\src\xpuDaemon\xpuDaemon.cpp`

### Issues:
1. Line 365: `OrchestrationManager::initialize()` called with 0 args but expects parameters
2. Line 372: `OrchestrationManager::startPipeline()` called with 0 args but expects parameters
3. Line 393: `OrchestrationManager::reloadConfiguration()` method doesn't exist

**Fix**: Check `OrchestrationManager.h` for correct method signatures and update calls accordingly.

## Quick PowerShell Commands to Apply Fixes

After the build completes and file locks are released:

```powershell
# Fix DaemonController.cpp
(Get-Content 'C:\workspace\cliMusic\xpu\src\xpuDaemon\DaemonController.cpp') -replace '\bpid_t\b', 'process_id_t' | Set-Content 'C:\workspace\cliMusic\xpu\src\xpuDaemon\DaemonController.cpp'

# Or use this Python script:
python -c "
with open('C:/workspace/cliMusic/xpu/src/xpuDaemon/DaemonController.cpp', 'r') as f:
    content = f.read()
content = content.replace('pid_t pid;', 'process_id_t pid;')
with open('C:/workspace/cliMusic/xpu/src/xpuDaemon/DaemonController.cpp', 'w') as f:
    f.write(content)
"
```

