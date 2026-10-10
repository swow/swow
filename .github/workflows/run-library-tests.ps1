param([int] $TimeoutMinutes = 5)

$ErrorActionPreference = 'Stop'
if (-not $env:GITHUB_WORKSPACE) {
    throw 'This script is intended to run in GitHub Actions only.'
}

function Stop-TestChildProcess([int] $ProcessId, [int] $TimeoutMilliseconds) {
    $terminator = [System.Diagnostics.Process]::new()
    $terminator.StartInfo.FileName = 'taskkill.exe'
    $terminator.StartInfo.UseShellExecute = $false
    $terminator.StartInfo.CreateNoWindow = $true
    foreach ($argument in @('/PID', $ProcessId.ToString(), '/T', '/F')) {
        $terminator.StartInfo.ArgumentList.Add($argument)
    }
    $started = $false
    try {
        $started = $terminator.Start()
        if (-not $started) {
            throw "Unable to start termination of test child $ProcessId."
        }
        if (-not $terminator.WaitForExit($TimeoutMilliseconds)) {
            Write-Warning "Termination of test child $ProcessId did not finish within the cleanup deadline."
        } elseif ($terminator.ExitCode -ne 0) {
            Write-Warning "Termination of test child $ProcessId exited with code $($terminator.ExitCode)."
        }
    } finally {
        if ($started -and -not $terminator.HasExited) {
            $terminator.Kill()
        }
        $terminator.Dispose()
    }
}

$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = 'C:\OpenCppCoverage\OpenCppCoverage.exe'
$startInfo.WorkingDirectory = $env:GITHUB_WORKSPACE
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
foreach ($argument in @(
    '--export_type', 'binary:lib_test.cov',
    '--sources', 'ext\include',
    '--sources', 'ext\src',
    '--excluded_sources', 'ext\deps',
    '--cover_children',
    '--excluded_line_regex', '\s*else.*',
    '--excluded_line_regex', '\s*\}.*',
    '--excluded_line_regex', '\s*\{\s*',
    '--modules', 'php_swow.dll',
    '--', 'C:\tools\php\php.exe', 'C:\tools\php\composer.phar',
    # PHPUnit 10 debug output exports complete successful assertion values; keep routine CI output compact.
    'test-library'
)) {
    $startInfo.ArgumentList.Add($argument)
}

$process = [System.Diagnostics.Process]::new()
$process.StartInfo = $startInfo
$started = $false
$exitCode = 1
try {
    $started = $process.Start()
    if (-not $started) {
        throw 'Unable to start library coverage tests.'
    }
    Write-Host "Library coverage process $($process.Id) started; test deadline: $TimeoutMinutes minutes."
    # Drain both redirected streams concurrently so verbose tests cannot block the process wait.
    Write-Host 'Starting library coverage output readers.'
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    Write-Host 'Waiting for the library coverage process to exit.'
    if ($process.WaitForExit([TimeSpan]::FromMinutes($TimeoutMinutes))) {
        $exitCode = $process.ExitCode
        Write-Host "Library coverage process exited with code $exitCode."
    } else {
        Write-Warning "Library tests exceeded the $TimeoutMinutes minute deadline."
        # A timeout remains a failure even if the coverage tool exits successfully during cleanup.
        $exitCode = 124
        # Stop this test's PHP descendants first so the coverage tool can finish its report.
        $cleanupTimer = [System.Diagnostics.Stopwatch]::StartNew()
        try {
            $children = Get-CimInstance Win32_Process -Filter "ParentProcessId=$($process.Id)" -OperationTimeoutSec 10
        } catch {
            Write-Warning "Unable to enumerate library coverage children: $_"
            $children = @()
        }
        foreach ($child in $children) {
            $remainingMilliseconds = 20000 - $cleanupTimer.ElapsedMilliseconds
            if ($remainingMilliseconds -le 0) {
                break
            }
            Write-Host "Stopping library coverage child process $($child.ProcessId)."
            Stop-TestChildProcess -ProcessId $child.ProcessId -TimeoutMilliseconds ([Math]::Min(10000, $remainingMilliseconds))
        }
        $null = $process.WaitForExit([TimeSpan]::FromSeconds(10))
    }
} finally {
    Write-Host 'Finalizing library coverage processes and output.'
    try {
        if ($started -and -not $process.HasExited) {
            $process.Kill($true)
            $null = $process.WaitForExit([TimeSpan]::FromSeconds(10))
        }
        if ($started) {
            if (-not [System.Threading.Tasks.Task]::WaitAll([System.Threading.Tasks.Task[]] @($stdout, $stderr), [TimeSpan]::FromSeconds(10))) {
                throw 'Library test output did not close after process cleanup.'
            }
            [Console]::Out.Write($stdout.Result)
            [Console]::Error.Write($stderr.Result)
        }
    } finally {
        $process.Dispose()
    }
}
exit $exitCode
