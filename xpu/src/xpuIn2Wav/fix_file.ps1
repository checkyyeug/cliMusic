$source = "C:\workspace\cliMusic\xpu\src\xpuIn2Wav\FFTEngine_fixed.cpp"
$destination = "C:\workspace\cliMusic\xpu\src\xpuIn2Wav\FFTEngine.cpp"
$maxAttempts = 60
$attempt = 0

while ($attempt -lt $maxAttempts) {
    try {
        Copy-Item -Path $source -Destination $destination -Force
        Write-Host "File replaced successfully!"
        exit 0
    } catch {
        Write-Host "Attempt $($attempt + 1): File still locked, waiting..."
        Start-Sleep -Seconds 2
        $attempt++
    }
}

Write-Host "Failed to replace file after $maxAttempts attempts"
exit 1
