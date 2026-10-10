param([int] $TimeoutMinutes = 5)

$ErrorActionPreference = 'Stop'
if (-not $env:GITHUB_WORKSPACE) {
    throw 'This script is intended to run in GitHub Actions only.'
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
    'test-library', '--', '--debug'
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
    # Drain both redirected streams concurrently so verbose tests cannot block the process wait.
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if ($process.WaitForExit([TimeSpan]::FromMinutes($TimeoutMinutes))) {
        $exitCode = $process.ExitCode
    } else {
        Write-Warning "Library tests exceeded the $TimeoutMinutes minute deadline."
        # A timeout remains a failure even if the coverage tool exits successfully during cleanup.
        $exitCode = 124
        # Stop this test's PHP descendants first so the coverage tool can finish its report.
        foreach ($child in (Get-CimInstance Win32_Process -Filter "ParentProcessId=$($process.Id)")) {
            & taskkill.exe /PID $child.ProcessId /T /F
        }
        $null = $process.WaitForExit([TimeSpan]::FromSeconds(10))
    }
} finally {
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
