# Test script to verify clean build warnings are reduced

Write-Host "Testing clean CMake configuration..." -ForegroundColor Green

# Remove build directory
Write-Host "Removing build directory..." -ForegroundColor Yellow
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue

# Create new build directory
New-Item -ItemType Directory -Path build | Out-Null
Set-Location build

# Configure
Write-Host "Running CMake configuration..." -ForegroundColor Yellow
$confOutput = cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON 2>&1

# Count warnings
$warnings = ($confOutput | Select-String -Pattern "warning" -CaseSensitive).Count
$deprecationWarnings = ($confOutput | Select-String -Pattern "Deprecation Warning" -CaseSensitive).Count

Write-Host "`n=== Configuration Summary ===" -ForegroundColor Cyan
Write-Host "Total warnings: $warnings"
Write-Host "Deprecation warnings: $deprecationWarnings"

if ($deprecationWarnings -eq 0) {
    Write-Host "SUCCESS: No deprecation warnings!" -ForegroundColor Green
} else {
    Write-Host "INFO: Some deprecation warnings still present (from external dependencies)" -ForegroundColor Yellow
}

Write-Host "`n=== Building ===" -ForegroundColor Cyan
cmake --build . --config Release -j | Out-Null
if ($LASTEXITCODE -eq 0) {
    Write-Host "Build completed successfully!" -ForegroundColor Green
} else {
    Write-Host "Build failed with errors" -ForegroundColor Red
}

Set-Location ..
