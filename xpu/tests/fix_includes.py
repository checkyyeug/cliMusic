import os
import re

# Mapping of incorrect paths to correct paths
path_mappings = {
    'xpu/src/xpuLoad/': '../../src/xpuLoad/',
    'xpu/src/xpuIn2Wav/': '../../src/xpuIn2Wav/',
    'xpu/src/xpuPlay/': '../../src/xpuPlay/',
    'xpu/src/xpuProcess/': '../../src/xpuProcess/',
    'xpu/src/xpuQueue/': '../../src/xpuQueue/',
    'xpu/src/xpuDaemon/': '../../src/xpuDaemon/',
    'xpu/src/lib/': '../../src/lib/',
}

def fix_includes(filepath):
    """Fix include statements in a test file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content

    # Fix double quotes issue (e.g., #include "path.h"")
    content = re.sub(r'#include\s+"([^"]+)""', r'#include "\1"', content)

    # Fix double quotes at end of line (e.g., #include "path.h"")
    content = re.sub(r'#include\s+"([^"]+)"\s*"', r'#include "\1"', content)

    # Fix missing quotes around includes
    content = re.sub(r'#include\s+xpu/src/([^\s]+)', r'#include "xpu/src/\1"', content)

    # Fix path mappings
    for old_path, new_path in path_mappings.items():
        content = content.replace(old_path, new_path)

    # Fix remaining xpu/src/ paths
    content = content.replace('xpu/src/', '../../src/')

    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Fixed: {filepath}")
        return True
    return False

def main():
    """Fix all test files"""
    test_dirs = ['unit', 'integration', 'crossplatform', 'error', 'performance']

    for test_dir in test_dirs:
        dir_path = os.path.join(os.path.dirname(__file__), test_dir)
        if not os.path.exists(dir_path):
            continue

        for filename in os.listdir(dir_path):
            if filename.endswith('.cpp'):
                filepath = os.path.join(dir_path, filename)
                fix_includes(filepath)

if __name__ == '__main__':
    main()
