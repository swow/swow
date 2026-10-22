# php deps downloader

param (
    [Object[]]$DllDeps,
    [int]$MaxTry = 3,
    [string]$ToolsPath = "C:\tools\phpdev",
    [string]$PhpBin = "php",
    [string]$PhpVer = "",
    [string]$PhpVCVer = "",
    [string]$PhpArch = "x64",
    [bool]$Staging = $false,
    [bool]$PhpTs = $false,
    [bool]$DryRun = $false
)

$scriptPath = Split-Path -parent $MyInvocation.MyCommand.Definition
. "$scriptPath\utils.ps1" -ToolName "deps" -MaxTry $MaxTry

if($DllDeps.Length -Lt 1){
    warn "No deps specified, just skipped."
    exit 0
}

# things we used later
$requiredCurlVersion = $null
if ("".Equals($PhpVer)){
    try{
        $PhpVer = & $PhpBin -r "echo PHP_MAJOR_VERSION . '.' . PHP_MINOR_VERSION . PHP_EOL;"
        $phpinfo = & $PhpBin -i
        $PhpVCVer = ($phpinfo | Select-String -Pattern 'PHP Extension Build => .+,(.+)' -CaseSensitive -List).Matches.Groups[1]
        $PhpArch = ($phpinfo | Select-String -Pattern 'Architecture => (.+)' -CaseSensitive -List).Matches.Groups[1]
        if ($DllDeps -contains 'libcurl') {
            $requiredCurlVersion = ([string] (& $PhpBin -r "echo extension_loaded('curl') ? curl_version()['version'] : '';")).Trim()
            if ($LASTEXITCODE -ne 0 -or ($requiredCurlVersion -and $requiredCurlVersion -notmatch '^\d+\.\d+\.\d+$')) {
                throw "Cannot determine the native PHP libcurl version"
            }
        }
    }finally{
        if(
            !$PhpVer -Or
            !$PhpVCVer -Or
            !$PhpArch
        ){
            err "Cannot determine php attributes, do you have php in PATH?"
            warn "phpver: $PhpVer"
            warn "phpvcver: $PhpVCVer"
            warn "phparch: $PhpArch"
            exit 1
        }
    }
}

$PhpVCVer = $PhpVCVer.ToLower()

info "Try to fetch deps series list from https://downloads.php.net/~windows"
if($Staging){
    $stagingStr = "staging"
}else{
    $stagingStr = "stable"
}
$seriesUrl = "https://downloads.php.net/~windows/php-sdk/deps/series/packages-$PhpVer-$PhpVCVer-$PhpArch-$stagingStr.txt"
info "Series URL: $seriesUrl"
$series = (fetchpage $seriesUrl).Content
if(!$series){
    err "Cannot get series information from https://downloads.php.net/~windows"
    exit 1
    # filelist may not match php distro, so disable it
    # $filelist = (fetchpage ("https://downloads.php.net/~windows/php-sdk/deps/" + $PhpVCVer.ToLower() + "/$PhpArch/")).Content
    # if(!$filelist){
    #     err "Neither series file nor file list can be got, aborting"
    #     exit 1
    # }
}

$downloadeddeps = [System.Collections.ArrayList]@()
foreach ($depname in $DllDeps) {
    $depfile = $null
    if($series){
        foreach ($filename in $series.Split()) {
            if($filename.StartsWith($depname)){
                info "Found file $filename for dep $depname"
                $depfile = $filename
                break
            }
        }
    }else{
        $fnver = searchfile $filelist -Pattern ("$depname-:-" + $PhpVCVer.ToLower() + "-$PhpArch.zip")
        $depfile = $fnver[0]
    }

    if(!$depfile){
        err "Cannot find dep file for $depname"
        exit 1
    }
    if ($depname -eq 'libcurl' -and $requiredCurlVersion) {
        # Swow links libcurl statically; the native PHP control must use the same
        # release, even when the floating SDK series has already moved ahead.
        $prefix = "libcurl-$requiredCurlVersion-"
        if (-not $depfile.StartsWith($prefix)) {
            $directory = "https://downloads.php.net/~windows/php-sdk/deps/$PhpVCVer/$PhpArch/"
            $page = fetchpage -Uri $directory
            $pattern = 'libcurl-' + [regex]::Escape($requiredCurlVersion) +
                '(?:-(?<revision>\d+))?-' + [regex]::Escape("$PhpVCVer-$PhpArch") + '\.zip'
            $package = [regex]::Matches($page.Content, $pattern) |
                Sort-Object -Property @{Expression = { [int] $_.Groups['revision'].Value }; Descending = $true} |
                Select-Object -First 1
            if (-not $package) {
                throw "Cannot find the exact native PHP libcurl $requiredCurlVersion package for $PhpVCVer-$PhpArch"
            }
            $depfile = $package.Value
        }
        info "Use native PHP libcurl $requiredCurlVersion package $depfile"
    }
    $downloadeddeps.Add($depfile) | Out-Null
    if($DryRun){
        continue
    }

    info "Downloading $depfile from https://downloads.php.net/~windows"
    provedir "$ToolsPath"
    provedir "$ToolsPath\deps"
    $dest = "$ToolsPath\deps\$depfile"
    if(Test-Path $dest -PathType Leaf){
        warn "$depfile is already provided, instant extract it."
    }else {
        $uri = "https://downloads.php.net/~windows/php-sdk/deps/$PhpVCVer/$PhpArch/$depfile"
        $ret = dlwithhash -Uri $uri -Dest $dest
        if (!$ret){
            err "Failed download $uri."
            exit 1
        }
    }
    info "Unzipping $dest to $ToolsPath\deps\"

    Expand-Archive $dest -Destination "$ToolsPath\deps\" -Force
    if ($depname -eq 'libcurl' -and $requiredCurlVersion) {
        $header = Get-Content -LiteralPath "$ToolsPath\deps\include\curl\curlver.h" -Raw
        if ($header -notmatch '#define\s+LIBCURL_VERSION\s+"([^"]+)"' -or $Matches[1] -ne $requiredCurlVersion) {
            throw "The extracted libcurl package does not match native PHP libcurl $requiredCurlVersion"
        }
    }
}

# if we are in github workflows, set downloaded as output
if(${env:CI} -Eq "true"){
    $s = ( $downloadeddeps | Sort-Object ) -Join "_"
    Write-Output "downloadeddeps=$s" | Out-File "${env:GITHUB_OUTPUT}" -Append
}

info "Done."

exit 0
