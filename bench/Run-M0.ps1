#Requires -Version 5.1
<#
.SYNOPSIS
    Measure Voxprint AI Lucent M0 startup on Windows.

.DESCRIPTION
    One command for the owner machine (Windows 11 or Windows Server 2025).

    A cold sample means: run right after reboot, with Microsoft Defender
    real-time protection left on. Only the first launch of each executable in
    that boot is a cold start. Runs 2..N reuse the Windows file cache. Ten
    independent cold medians would need ten reboots, so this script records
    run 1 on its own and also the median and nearest-rank p95 of the N runs.

    Warm medians use 250 ms for JPEG and WebP, and 350 ms for AVIF and JPEG XL
    at 12 megapixels or larger. That result is warm_gate. The cold JPEG
    first-launch gate stays 250 ms. Cached next/previous stays 30 ms.

    -ScheduleCold registers a one-shot AtLogOn task named
    VoxprintLucent-M0-Cold, then tells you to reboot. The task runs as the
    user who registered it, with its working directory set to this script's
    folder. It runs -ColdResume once and unregisters itself only after a
    results file exists. A start and outcome line is always appended to
    results\m0-task.log next to this script, and to
    %LOCALAPPDATA%\Voxprint\Lucent\m0-task.log.

    If you reboot and start this script yourself while uptime is still under
    -ColdUptimeSeconds (default 180), that invocation is treated as the cold
    resume and you do not need the task.

    Results: a timestamped .log and .json under results\ (next to an unpacked
    harness) or bench\results\ (in a git checkout).

    The harness zip's embeddable Python ignores PYTHONPATH. The build lists
    ..\app in python\python*._pth so -m voxprint_lucent imports the staged
    tree. A git checkout uses a normal interpreter and the PYTHONPATH this
    script sets.

.PARAMETER ScheduleCold
    Register the post-reboot task and exit without launching the viewer.

.PARAMETER ColdResume
    Take the cold sample, then the warm samples. The scheduled task uses this.

.PARAMETER WarmOnly
    Skip the cold gate and measure warm runs only.

.PARAMETER Runs
    Samples per format after the protocol above. Default 10.

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File .\Run-M0.ps1

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File .\bench\Run-M0.ps1 -ScheduleCold
#>
[CmdletBinding()]
param(
    [switch]$ScheduleCold,
    [switch]$ColdResume,
    [switch]$WarmOnly,
    [int]$Runs = 10,
    [int]$ColdUptimeSeconds = 180,
    [string]$CppExe,
    [string]$PythonExe,
    [string]$AppDir,
    [string]$ImageDir,
    [string]$OutDir
)

$ErrorActionPreference = 'Stop'
$TaskName = 'VoxprintLucent-M0-Cold'
$BudgetMs = 250.0
$FullResBudgetMs = 350.0
$NavigateBudgetMs = 30.0
$Utf8NoBom = New-Object System.Text.UTF8Encoding $false
try { [Console]::InputEncoding = $Utf8NoBom } catch { }
try { [Console]::OutputEncoding = $Utf8NoBom } catch { }
$OutputEncoding = $Utf8NoBom

function Write-Utf8NoBom([string]$Path, [string]$Text) {
    [System.IO.File]::WriteAllText($Path, $Text, $Utf8NoBom)
}

function Write-TaskLog([string]$Message) {
    $line = '{0} {1}' -f (Get-Date).ToString('o'), $Message
    $dirs = @()
    if ($PSScriptRoot) { $dirs += (Join-Path $PSScriptRoot 'results') }
    if ($env:LOCALAPPDATA) { $dirs += (Join-Path $env:LOCALAPPDATA 'Voxprint\Lucent') }
    foreach ($dir in $dirs) {
        try {
            New-Item -ItemType Directory -Force -Path $dir | Out-Null
            Add-Content -LiteralPath (Join-Path $dir 'm0-task.log') -Value $line -Encoding ASCII
        } catch {
        }
    }
}

function Quote-Argument([string]$Value) {
    if ($Value -match '[\s"]') {
        return '"' + ($Value.Replace('"', '\"')) + '"'
    }
    return $Value
}

function Get-Median([double[]]$Values) {
    $sorted = @($Values | Sort-Object)
    $count = $sorted.Count
    if ($count -eq 0) { return $null }
    if ($count % 2 -eq 1) {
        return [double]$sorted[[int](($count - 1) / 2)]
    }
    $lower = [double]$sorted[($count / 2) - 1]
    $upper = [double]$sorted[$count / 2]
    return ($lower + $upper) / 2.0
}

function Get-NearestRankP95([double[]]$Values) {
    $sorted = @($Values | Sort-Object)
    $count = $sorted.Count
    if ($count -eq 0) { return $null }
    $rank = [int][Math]::Ceiling(0.95 * $count)
    return [double]$sorted[$rank - 1]
}

function Resolve-Layout {
    $here = $PSScriptRoot
    $repo = Split-Path -Parent $here
    $root = $null
    $cpp = $CppExe
    $python = $PythonExe
    $app = $AppDir
    $images = $ImageDir
    $share = $null
    $results = $OutDir

    if (Test-Path -LiteralPath (Join-Path $here 'voxprint-lucent.exe')) {
        $root = $here
        if (-not $cpp) { $cpp = Join-Path $here 'voxprint-lucent.exe' }
        if (-not $python) { $python = Join-Path $here 'python\python.exe' }
        if (-not $app) { $app = Join-Path $here 'app' }
        if (-not $images) { $images = Join-Path $here 'images' }
        $share = Join-Path $here 'share'
        if (-not $results) { $results = Join-Path $here 'results' }
    } else {
        $packed = Join-Path $repo 'dist\m0-harness\voxprint-lucent.exe'
        if (-not $cpp -and (Test-Path -LiteralPath $packed)) {
            $cpp = $packed
            $root = Join-Path $repo 'dist\m0-harness'
        }
        if (-not $root) { $root = $repo }
        if (-not $python) {
            $venv = Join-Path $repo '.venv\Scripts\python.exe'
            if (Test-Path -LiteralPath $venv) { $python = $venv }
        }
        if (-not $app) { $app = $repo }
        if (-not $images) { $images = Join-Path $repo 'bench\fixtures' }
        $share = Join-Path $repo 'share'
        if (-not $results) { $results = Join-Path $repo 'bench\results' }
        if (-not $cpp) {
            $candidates = @(
                (Join-Path $repo 'cpp\build\Release\voxprint-lucent.exe'),
                (Join-Path $repo 'cpp\build\voxprint-lucent.exe')
            )
            foreach ($candidate in $candidates) {
                if (Test-Path -LiteralPath $candidate) { $cpp = $candidate; break }
            }
        }
    }

    return @{
        Root = $root
        Cpp = $cpp
        Python = $python
        App = $app
        Images = $images
        Share = $share
        Results = $results
    }
}

function Get-UptimeSeconds {
    $boot = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
    return ((Get-Date) - $boot).TotalSeconds
}

function Get-EnvironmentReport {
    $cv = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
    $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
    $gpus = @(Get-CimInstance Win32_VideoController | ForEach-Object {
            @{
                name = $_.Name
                driver_version = $_.DriverVersion
                ram_bytes = $_.AdapterRAM
            }
        })
    $defender = @{ available = $false; real_time_protection = $null; antivirus_enabled = $null; error = $null }
    try {
        $mp = Get-MpComputerStatus
        $defender.available = $true
        $defender.real_time_protection = [bool]$mp.RealTimeProtectionEnabled
        $defender.antivirus_enabled = [bool]$mp.AntivirusEnabled
    } catch {
        $defender.error = $_.Exception.Message
    }
    return @{
        os_product = [string]$cv.ProductName
        os_display_version = [string]$cv.DisplayVersion
        os_build = "$($cv.CurrentBuild).$($cv.UBR)"
        os_arch = $env:PROCESSOR_ARCHITECTURE
        cpu = [string]$cpu.Name
        cpu_cores = [int]$cpu.NumberOfCores
        cpu_logical = [int]$cpu.NumberOfLogicalProcessors
        gpus = $gpus
        defender = $defender
        uptime_seconds = [math]::Round((Get-UptimeSeconds), 1)
        powershell = $PSVersionTable.PSVersion.ToString()
    }
}

function Find-ImageFile([string]$Directory, [string[]]$Names) {
    foreach ($name in $Names) {
        $path = Join-Path $Directory $name
        if (Test-Path -LiteralPath $path) { return (Resolve-Path -LiteralPath $path).Path }
    }
    return $null
}

function Ensure-Images([hashtable]$Layout) {
    New-Item -ItemType Directory -Force -Path $Layout.Images | Out-Null
    $jpeg = Find-ImageFile $Layout.Images @('large.jpg', 'jpeg12.jpg')
    if ($jpeg) { return }
    if (-not $Layout.Python -or -not (Test-Path -LiteralPath $Layout.Python)) {
        throw "No test images in $($Layout.Images) and no Python to generate them."
    }
    $env:PYTHONPATH = $Layout.App
    & $Layout.Python -c "from pathlib import Path; from bench.fixtures import ensure_fixtures; ensure_fixtures(Path(r'$($Layout.Images)'))"
    if ($LASTEXITCODE -ne 0) {
        throw "Could not generate fixtures (exit $LASTEXITCODE)."
    }
}

function Measure-Launch {
    param(
        [string]$FileName,
        [string[]]$Arguments,
        [hashtable]$ExtraEnv,
        [string]$WorkingDirectory,
        [int]$TimeoutSec = 90
    )
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FileName
    $quoted = foreach ($arg in $Arguments) { Quote-Argument $arg }
    $psi.Arguments = ($quoted -join ' ')
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.StandardOutputEncoding = $Utf8NoBom
    $psi.StandardErrorEncoding = $Utf8NoBom
    $psi.CreateNoWindow = $true
    if ($ExtraEnv) {
        foreach ($key in @($ExtraEnv.Keys)) {
            $psi.EnvironmentVariables[$key] = [string]$ExtraEnv[$key]
        }
    }
    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    $watch = New-Object System.Diagnostics.Stopwatch
    $watch.Start()
    [void]$proc.Start()
    $errTask = $proc.StandardError.ReadToEndAsync()
    $first = $null
    $nav = $null
    $phase = $null
    $pending = $proc.StandardOutput.ReadLineAsync()
    $timedOut = $false
    $wantNav = $Arguments -contains '--navigate-next'
    while ($true) {
        if ($pending.Wait(200)) {
            $line = $pending.Result
            if ($null -eq $line) { break }
            if ($line.StartsWith('FIRST_PIXEL') -and $null -eq $first) {
                $first = [math]::Round($watch.Elapsed.TotalMilliseconds, 3)
            } elseif ($line.StartsWith('NAVIGATE_MS')) {
                $parts = $line.Split(' ', [System.StringSplitOptions]::RemoveEmptyEntries)
                if ($parts.Length -ge 2) { $nav = [double]$parts[1] }
            } elseif ($line.StartsWith('LUCENT_PHASE')) {
                $phase = $line
            }
            if ($null -ne $first -and (($null -ne $nav) -or (-not $wantNav))) { break }
            $pending = $proc.StandardOutput.ReadLineAsync()
        }
        if ($watch.Elapsed.TotalSeconds -ge $TimeoutSec) {
            $timedOut = $true
            try { $proc.Kill() } catch { }
            break
        }
    }
    if (-not $proc.HasExited) {
        if (-not $proc.WaitForExit(8000)) {
            try { $proc.Kill() } catch { }
            $timedOut = $true
        }
    }
    $stderr = ''
    try { $stderr = [string]$errTask.Result } catch { $stderr = $_.Exception.Message }
    $code = -1
    if ($proc.HasExited) { $code = [int]$proc.ExitCode }
    return @{
        first_pixel_ms = $first
        navigate_ms = $nav
        phase = $phase
        exit_code = $code
        timed_out = $timedOut
        stderr = $stderr.Trim()
    }
}

function Summarize-Samples([object[]]$Samples) {
    $times = @($Samples | Where-Object { $null -ne $_.first_pixel_ms } | ForEach-Object { [double]$_.first_pixel_ms })
    $navs = @($Samples | Where-Object { $null -ne $_.navigate_ms } | ForEach-Object { [double]$_.navigate_ms })
    $summary = @{
        samples = @($Samples)
        run_1_ms = $null
        median_ms = $null
        p95_ms = $null
        navigate_median_ms = $null
        errors = @($Samples | Where-Object { $null -eq $_.first_pixel_ms } | ForEach-Object { $_.stderr })
    }
    if ($Samples.Count -gt 0) {
        $summary.run_1_ms = $Samples[0].first_pixel_ms
    }
    if ($times.Count -gt 0) {
        $summary.median_ms = Get-Median $times
        $summary.p95_ms = Get-NearestRankP95 $times
    }
    if ($navs.Count -gt 0) {
        $summary.navigate_median_ms = Get-Median $navs
    }
    return $summary
}

function Get-Gate([object]$Summary, [bool]$ColdBoot) {
    if (-not $ColdBoot -or $null -eq $Summary.run_1_ms) { return 'unmeasured' }
    if ([double]$Summary.run_1_ms -le $BudgetMs) { return 'pass' } else { return 'fail' }
}

function Get-WarmBudgetMs([string]$FormatName) {
    if ($FormatName -eq 'avif' -or $FormatName -eq 'jxl') { return $FullResBudgetMs }
    return $BudgetMs
}

function Get-WarmGate([object]$Summary, [string]$FormatName) {
    if ($null -eq $Summary.median_ms) { return $null }
    if ([double]$Summary.median_ms -le (Get-WarmBudgetMs $FormatName)) { return 'pass' } else { return 'fail' }
}

function Register-ColdTask {
    $arg = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -ColdResume"
    $workdir = $PSScriptRoot
    $user = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    $action = New-ScheduledTaskAction -Execute "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -Argument $arg -WorkingDirectory $workdir
    $trigger = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
    $principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Limited
    Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger -Principal $principal -Force | Out-Null
    Write-TaskLog "registered task=$TaskName user=$user workdir=$workdir"
}

function Get-SampleReason([object]$Summary) {
    foreach ($text in @($Summary.errors)) {
        if (-not $text) { continue }
        $lines = @(([string]$text) -split '\r?\n' | Where-Object { $_.Trim() })
        if ($lines.Count -eq 0) { continue }
        $line = $lines[-1].Trim()
        if ($line.Length -gt 240) { $line = $line.Substring(0, 240) }
        return $line
    }
    return 'no first-pixel sample'
}

function Unregister-ColdTask {
    # The script is the running task, so Unregister-ScheduledTask throws until
    # this process exits. Log that, then ask a second process to remove it.
    $logPaths = @()
    if ($PSScriptRoot) { $logPaths += (Join-Path $PSScriptRoot 'results\m0-task.log') }
    if ($env:LOCALAPPDATA) { $logPaths += (Join-Path $env:LOCALAPPDATA 'Voxprint\Lucent\m0-task.log') }
    Write-TaskLog "outcome=unregister-attempt task=$TaskName"
    $existing = $null
    try {
        $existing = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
    } catch {
        $detail = ($_.Exception.Message -replace '[\r\n]+', ' ')
        Write-TaskLog "outcome=unregister task=$TaskName detail=query-failed error=$detail"
        return
    }
    if (-not $existing) {
        Write-TaskLog "outcome=unregistered task=$TaskName detail=already-absent"
        return
    }
    try {
        Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false -ErrorAction Stop
        Write-TaskLog "outcome=unregistered task=$TaskName"
    } catch {
        $detail = ($_.Exception.Message -replace '[\r\n]+', ' ')
        Write-TaskLog "outcome=unregister task=$TaskName detail=deferred error=$detail"
        $helper = Join-Path $env:TEMP 'lucent-m0-unregister.ps1'
        $lines = @(
            'Start-Sleep -Seconds 5',
            "Unregister-ScheduledTask -TaskName '$TaskName' -Confirm:`$false -ErrorAction SilentlyContinue",
            "if (Get-ScheduledTask -TaskName '$TaskName' -ErrorAction SilentlyContinue) { exit 1 }"
        )
        foreach ($path in $logPaths) {
            $escaped = $path.Replace("'", "''")
            $lines += "Add-Content -LiteralPath '$escaped' -Value ((Get-Date).ToString('o') + ' outcome=unregistered task=$TaskName detail=deferred-helper') -Encoding ASCII"
        }
        $lines += 'exit 0'
        Set-Content -LiteralPath $helper -Value ($lines -join "`r`n") -Encoding ASCII
        Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-WindowStyle', 'Hidden', '-File', $helper) -WindowStyle Hidden
    }
}

Write-TaskLog ("start user={0} cwd={1} script={2} ScheduleCold={3} ColdResume={4}" -f `
        [Security.Principal.WindowsIdentity]::GetCurrent().Name, (Get-Location).Path, $PSCommandPath, [bool]$ScheduleCold, [bool]$ColdResume)
$layout = Resolve-Layout
New-Item -ItemType Directory -Force -Path $layout.Results | Out-Null

if ($ScheduleCold) {
    try {
        Register-ColdTask
    } catch {
        Write-Host "Could not register scheduled task '$TaskName': $($_.Exception.Message)"
        Write-Host "Reboot, sign in, and immediately run:"
        Write-Host "  powershell -NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -ColdResume"
        Write-Host "Leave Defender real-time protection on. Do not open the viewer before the script."
        exit 1
    }
    Write-Host "Scheduled task '$TaskName' will run once at the next logon, then remove itself."
    Write-Host "Reboot now. Leave Microsoft Defender real-time protection ON."
    Write-Host "Sign in and do not start the viewer yourself; the task is the first launch."
    Write-Host "A cold sample is that first launch. Later runs in the same boot are not cold."
    exit 0
}

$uptime = Get-UptimeSeconds
$coldBoot = $false
if (-not $WarmOnly) {
    if ($ColdResume -or $uptime -le $ColdUptimeSeconds) {
        $coldBoot = $true
    }
}

if ($ColdResume -and $uptime -gt 600) {
    Write-Host "Uptime is $([int]$uptime)s. This boot is too old to call the first launch cold."
    $coldBoot = $false
}

Write-Host "Cold gate: $(if ($coldBoot) { 'this boot qualifies (run right after reboot, Defender on)' } else { 'NOT a cold boot' })"
if (-not $coldBoot -and -not $WarmOnly) {
    Write-Host "For a cold sample, reboot with Defender real-time protection on, then either:"
    Write-Host "  powershell -NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -ScheduleCold"
    Write-Host "or, right after you sign in (uptime under ${ColdUptimeSeconds}s), run this script with no switches."
}

Ensure-Images $layout
$formats = @(
    @{ name = 'jpeg'; file = (Find-ImageFile $layout.Images @('large.jpg', 'jpeg12.jpg')); neighbour = (Find-ImageFile $layout.Images @('large-next.jpg', 'jpeg12-next.jpg')) },
    @{ name = 'webp'; file = (Find-ImageFile $layout.Images @('large.webp', 'webp12.webp')); neighbour = $null },
    @{ name = 'avif'; file = (Find-ImageFile $layout.Images @('large.avif', 'avif12.avif')); neighbour = $null },
    @{ name = 'jxl'; file = (Find-ImageFile $layout.Images @('large.jxl', 'jxl12.jxl')); neighbour = $null }
)

$targets = @()
if ($layout.Cpp -and (Test-Path -LiteralPath $layout.Cpp)) {
    $targets += @{ name = 'cpp'; file = $layout.Cpp; arguments = @(); env = @{} }
}
if ($layout.Python -and (Test-Path -LiteralPath $layout.Python)) {
    $pyEnv = @{ PYTHONPATH = [string]$layout.App; PYTHONUNBUFFERED = '1' }
    if ($layout.Share) { $pyEnv['LUCENT_SHARE'] = [string]$layout.Share }
    $targets += @{ name = 'pyside6'; file = $layout.Python; arguments = @('-m', 'voxprint_lucent'); env = $pyEnv }
}
if ($targets.Count -eq 0) {
    throw "No voxprint-lucent.exe and no Python. Unzip the lucent-m0-windows artifact and run Run-M0.ps1 from that folder."
}

function Invoke-Profile([hashtable]$Target, [hashtable]$FormatInfo, [bool]$Warmup) {
    $launchArgs = @($Target.arguments) + @('--benchmark', [string]$FormatInfo.file)
    if ($FormatInfo.neighbour) {
        $launchArgs += @('--navigate-next', [string]$FormatInfo.neighbour)
    }
    if ($Warmup) {
        Measure-Launch -FileName $Target.file -Arguments $launchArgs -ExtraEnv $Target.env -WorkingDirectory $layout.Root | Out-Null
    }
    $samples = @()
    for ($i = 0; $i -lt $Runs; $i++) {
        $samples += Measure-Launch -FileName $Target.file -Arguments $launchArgs -ExtraEnv $Target.env -WorkingDirectory $layout.Root
    }
    return Summarize-Samples $samples
}

$environment = Get-EnvironmentReport
$reportTargets = @()
$firstExecutable = $true
$ResultsWritten = $false
try {
    foreach ($target in $targets) {
        $formatReports = @()
        foreach ($fmt in $formats) {
            if (-not $fmt.file) {
                $formatReports += @{ name = $fmt.name; status = 'skipped'; reason = 'image missing'; error = 'image missing' }
                continue
            }
            Write-Host ("{0} {1}: {2} x {3}" -f $target.name, $fmt.name, $(if ($coldBoot) { 'session-after-boot' } else { 'session' }), $Runs)
            $session = Invoke-Profile $target $fmt $false
            Write-Host ("{0} {1}: warm x {2}" -f $target.name, $fmt.name, $Runs)
            $warm = Invoke-Profile $target $fmt $true
            $basis = 'not_cold'
            if ($coldBoot -and $firstExecutable -and $fmt.name -eq 'jpeg') {
                $basis = 'first_launch_after_reboot'
            } elseif ($coldBoot) {
                $basis = 'same_boot_file_cache_already_warm'
            }
            $navVerdict = $null
            if ($null -ne $warm.navigate_median_ms) {
                if ([double]$warm.navigate_median_ms -le $NavigateBudgetMs) { $navVerdict = 'pass' } else { $navVerdict = 'fail' }
            }
            $status = 'measured'
            $reason = $null
            if ($null -eq $session.median_ms -and $null -eq $warm.median_ms) {
                $status = 'skipped'
                $reason = Get-SampleReason $session
                if ($reason -eq 'no first-pixel sample') { $reason = Get-SampleReason $warm }
            }
            $formatReports += @{
                name = $fmt.name
                status = $status
                reason = $reason
                image = [string]$fmt.file
                cold_basis = $basis
                cold_run_1_ms = $session.run_1_ms
                cold_gate = $(if ($basis -eq 'first_launch_after_reboot') { Get-Gate $session $true } else { 'unmeasured' })
                session_runs_ms = @($session.samples | ForEach-Object { $_.first_pixel_ms })
                session_median_ms = $session.median_ms
                session_p95_ms = $session.p95_ms
                session_errors = @($session.errors)
                warm_runs_ms = @($warm.samples | ForEach-Object { $_.first_pixel_ms })
                warm_median_ms = $warm.median_ms
                warm_p95_ms = $warm.p95_ms
                warm_budget_ms = (Get-WarmBudgetMs $fmt.name)
                warm_gate = (Get-WarmGate $warm $fmt.name)
                warm_errors = @($warm.errors)
                navigate_median_ms = $warm.navigate_median_ms
                navigate_gate = $navVerdict
            }
        }
        $allSkipped = $true
        $targetReason = $null
        foreach ($fmt in $formatReports) {
            if ($fmt.status -ne 'skipped') { $allSkipped = $false }
            elseif (-not $targetReason -and $fmt.reason) { $targetReason = [string]$fmt.reason }
        }
        if (-not $allSkipped) { $targetReason = $null }
        $reportTargets += @{
            name = $target.name
            executable = $target.file
            status = $(if ($allSkipped) { 'skipped' } else { 'measured' })
            reason = $targetReason
            formats = $formatReports
        }
        $firstExecutable = $false
    }

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$payload = @{
    created_local = (Get-Date).ToString('o')
    budget_first_pixel_ms = $BudgetMs
    budget_full_res_warm_ms = $FullResBudgetMs
    budget_navigate_ms = $NavigateBudgetMs
    runs = $Runs
    cold_boot = $coldBoot
    methodology = @(
        'Cold means run right after reboot with Microsoft Defender real-time protection on.',
        'Only the first launch of the C++ executable in that boot is a cold start of voxprint-lucent. The pass/fail field cold_gate uses that sample (jpeg, run 1), not the median.',
        'N runs in the same boot are session numbers. Runs after the first are warm in the Windows file cache. A median of 10 independent cold starts would need 10 reboots.',
        'p95 is nearest-rank (for N=10 that is the maximum). The median of an even count is the average of the two middle values.',
        'Warm numbers follow one unmeasured warmup launch. JPEG and WebP warm median must be at most 250 ms. AVIF and JPEG XL at 12 megapixels or larger must be at most 350 ms warm. That field is warm_gate. PySide6 is the comparison harness and is expected to miss both.'
    )
    environment = $environment
    targets = $reportTargets
}
$jsonPath = Join-Path $layout.Results "m0-$stamp.json"
$logPath = Join-Path $layout.Results "m0-$stamp.log"
Write-Utf8NoBom $jsonPath (($payload | ConvertTo-Json -Depth 8) + "`n")

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("Voxprint AI Lucent M0")
$lines.Add("created: $($payload.created_local)")
$lines.Add("cold boot: $coldBoot")
$lines.Add("budget: first visible pixel <= $BudgetMs ms; full-res AVIF and JXL warm <= $FullResBudgetMs ms; cached navigate <= $NavigateBudgetMs ms")
$envr = $payload.environment
$lines.Add("OS: $($envr.os_product) $($envr.os_display_version) build $($envr.os_build) $($envr.os_arch)")
$lines.Add("CPU: $($envr.cpu) cores=$($envr.cpu_cores) logical=$($envr.cpu_logical)")
foreach ($gpu in @($envr.gpus)) {
    $lines.Add("GPU: $($gpu.name) driver $($gpu.driver_version)")
}
$def = $envr.defender
$lines.Add("Defender: available=$($def.available) real_time=$($def.real_time_protection) antivirus=$($def.antivirus_enabled) error=$($def.error)")
$lines.Add("uptime_seconds: $($envr.uptime_seconds)")
$lines.Add("p95: nearest-rank. Even-N median: average of the two middle samples.")
$lines.Add("cold_gate is pass/fail for the first C++ JPEG launch after reboot only. Other rows are unmeasured for that gate.")
foreach ($target in $reportTargets) {
    $lines.Add("")
    $lines.Add("== $($target.name) ==")
    $lines.Add("exe: $($target.executable)")
    foreach ($fmt in @($target.formats)) {
        if ($fmt.error) {
            $lines.Add("$($fmt.name): $($fmt.error)")
            continue
        }
        $lines.Add(("{0}: cold_run_1={1} cold_gate={2} basis={3} session_median={4} session_p95={5} warm_median={6} warm_p95={7} warm_budget={8} warm_gate={9} navigate_median={10} navigate_gate={11}" -f `
                    $fmt.name, $fmt.cold_run_1_ms, $fmt.cold_gate, $fmt.cold_basis, $fmt.session_median_ms, $fmt.session_p95_ms, `
                    $fmt.warm_median_ms, $fmt.warm_p95_ms, $fmt.warm_budget_ms, $fmt.warm_gate, $fmt.navigate_median_ms, $fmt.navigate_gate))
        $lines.Add("  session_ms: $($fmt.session_runs_ms -join ', ')")
        $lines.Add("  warm_ms: $($fmt.warm_runs_ms -join ', ')")
        if ($fmt.status -eq 'skipped' -and $fmt.reason) {
            $lines.Add("  status=skipped reason=$($fmt.reason)")
        }
        if (@($fmt.session_errors).Count -gt 0) {
            $lines.Add("  session_errors: $($fmt.session_errors -join ' | ')")
        }
    }
}
$lines.Add("")
$lines.Add("json: $jsonPath")
Write-Utf8NoBom $logPath (($lines -join "`r`n") + "`r`n")
if ((Test-Path -LiteralPath $jsonPath) -and (Test-Path -LiteralPath $logPath)) {
    $ResultsWritten = $true
    Write-TaskLog "outcome=wrote path=$jsonPath"
}
Write-Host "Wrote $logPath"
Write-Host "Wrote $jsonPath"
} catch {
    $message = $_.Exception.Message
    Write-TaskLog "failed: $message"
    try {
        $errDir = $layout.Results
        if (-not $errDir) { $errDir = Join-Path $PSScriptRoot 'results' }
        New-Item -ItemType Directory -Force -Path $errDir | Out-Null
        $errPath = Join-Path $errDir ("m0-error-{0}.json" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
        @{
            error = $message
            script = $PSCommandPath
            cwd = (Get-Location).Path
            user = [Security.Principal.WindowsIdentity]::GetCurrent().Name
        } | ConvertTo-Json | Set-Content -LiteralPath $errPath -Encoding ASCII
        if (Test-Path -LiteralPath $errPath) {
            $ResultsWritten = $true
            Write-TaskLog "wrote $errPath"
        }
    } catch {
        Write-TaskLog "could not write error json: $($_.Exception.Message)"
    }
    throw
} finally {
    if ($ColdResume -and $ResultsWritten) {
        Unregister-ColdTask
    } elseif ($ColdResume) {
        Write-TaskLog "outcome=left-registered task=$TaskName detail=no-results-file"
    }
}
exit 0
