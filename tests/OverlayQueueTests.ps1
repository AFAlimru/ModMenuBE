param([ValidateSet('debug','release')][string]$Mode='release')
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$evidence=Join-Path $repoRoot ('artifacts/overlay-queue-tests/'+$Mode+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence | Out-Null
$exe=Join-Path $repoRoot "build/windows/x64/$Mode/OverlayQueueHost.exe"
$dll=Join-Path $repoRoot "build/windows/x64/$Mode/OverlayQueueFixture.dll"
$checks=0;$failures=0
foreach($scenario in @('hwnd','legacy','composition','resize','live-resize','multi-resize','failed-resize','failed-create','late','d3d11','f11-resize','f11-d3d11','f11-lock','cleanup-timeout','unrelated-resize','f11-hardware','f11-empty','present-forwarding','early-observer','observer-repeat','observer-abort','observer-reinstall','observer-retry','early-d3d11','observer-partial','window-rebind','window-destroy','observer-fallback-d3d11','observer-fallback-d3d12','mouse-polling-gate')){
    # Only this inert, hidden host is run, never a game or shipping plugin.
    $child=Start-Process -FilePath $exe -ArgumentList ('"'+$dll+'" '+$scenario) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $evidence ($scenario+'.txt')) -RedirectStandardError (Join-Path $evidence ($scenario+'-stderr.txt'))
    $null=$child.Handle
    try{
        if(-not $child.WaitForExit(15000)){
            Stop-Process -Id $child.Id
            $failures++;Write-Output "FAIL $scenario inert fixture timed out"
        }elseif($child.ExitCode -ne 0){$failures++;Write-Output "FAIL $scenario returned $($child.ExitCode)"}
        $checks++
        Get-Content -LiteralPath (Join-Path $evidence ($scenario+'.txt')) | Write-Output
        Get-Content -LiteralPath (Join-Path $evidence ($scenario+'-stderr.txt')) | Write-Output
    }finally{$child.Dispose()}
}
Write-Output "$checks actual overlay scenarios / $failures failures"
if($failures){exit 1}
