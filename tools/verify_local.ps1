param([ValidateSet('debug','release')][string]$Mode='release',[string]$ImagesRoot='')
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
Push-Location $root
try {
    & xmake f -y -m $Mode
    if($LASTEXITCODE -ne 0){throw 'Configure failed'}
    & xmake build -y -j 4 -a
    if($LASTEXITCODE -ne 0){throw 'Build failed'}
    $evidence=Join-Path $root 'artifacts\verification'
    New-Item -ItemType Directory -Path $evidence -Force | Out-Null
    foreach($test in @('DefinitionTests','NativeTests','Native51VariantsTests','ExternalSettingsTests','SettingsTransactionTests','TextEditorTests','LocalIntegrationTests','D3D12RuntimeTests','WorkspaceEntryTests','WorkspaceProtocolTests','WorkspaceHandoffTests','WorkspaceApiTests','WorkspaceViewTests','WorkspaceUiTests')){
        $result=& (Join-Path $root "build\windows\x64\$Mode\$test.exe") 2>&1
        $code=$LASTEXITCODE
        $result | Tee-Object -LiteralPath (Join-Path $evidence "$Mode-$test.txt")
        if($code -ne 0){throw "$test failed ($code)"}
    }
    $result=& py tests/LoaderDependencyTests.py (Join-Path $root "build/windows/x64/$Mode/ModMenuBE.dll") 2>&1
    $code=$LASTEXITCODE
    $result | Tee-Object -LiteralPath (Join-Path $evidence "$Mode-LoaderDependencyTests.txt")
    if($code -ne 0){throw "Loader dependency audit failed ($code)"}
    if($ImagesRoot){
        $result=& (Join-Path $root "build\windows\x64\$Mode\NativeTests.exe") $ImagesRoot 2>&1
        $code=$LASTEXITCODE
        $result | Tee-Object -LiteralPath (Join-Path $evidence "$Mode-images.txt")
        if($code -ne 0){throw "Private image verification failed ($code)"}
        foreach($version in @('1.26.45.01','1.26.52.03')){
            $shadow=Join-Path $ImagesRoot "$version/d3d12.dll"
            if(Test-Path -LiteralPath $shadow){
                $result=& (Join-Path $root "build/windows/x64/$Mode/D3D12RuntimeTests.exe") $shadow 2>&1
                $code=$LASTEXITCODE
                $result | Tee-Object -LiteralPath (Join-Path $evidence "$Mode-D3D12-shadow-$version.txt")
                if($code -ne 0){throw "App-local dependency regression failed ($version, $code)"}
            }
        }
    }
    $dll=Get-Item -LiteralPath (Join-Path $root "build/windows/x64/$Mode/ModMenuBE.dll")
    [ordered]@{
        mode=$Mode
        dll_sha256=(Get-FileHash -LiteralPath $dll.FullName -Algorithm SHA256).Hash
        dll_bytes=$dll.Length
        executable_suites=14
        dependency_audit='passed'
        private_images=[bool]$ImagesRoot
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence "$Mode-summary.json") -Encoding utf8
}finally{Pop-Location}
