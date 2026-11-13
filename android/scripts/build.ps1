param(
    [ValidateSet("arm64-v8a", "armeabi-v7a")]
    [string]$Abi = "arm64-v8a",
    [string]$Sdk = "$env:USERPROFILE\Android\Sdk"
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$ndk = Get-ChildItem (Join-Path $Sdk "ndk") -Directory | Sort-Object Name -Descending | Select-Object -First 1
$cmakeBin = Get-ChildItem (Join-Path $Sdk "cmake") -Directory | Sort-Object Name -Descending | Select-Object -First 1
$cmake = Join-Path $cmakeBin.FullName "bin\cmake.exe"
$ninja = Join-Path $cmakeBin.FullName "bin\ninja.exe"
$build = Join-Path $root "build-$Abi"

& $cmake -S $root -B $build -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$($ninja -replace '\\','/')" `
    "-DCMAKE_TOOLCHAIN_FILE=$(($ndk.FullName -replace '\\','/'))/build/cmake/android.toolchain.cmake" `
    "-DANDROID_ABI=$Abi" "-DANDROID_PLATFORM=android-28" "-DANDROID_STL=c++_shared" `
    "-DCMAKE_BUILD_TYPE=Release"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmake --build $build --target levi_package
exit $LASTEXITCODE
