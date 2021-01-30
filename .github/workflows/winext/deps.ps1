# php deps downloader

param (
    [Object[]]$DllDeps,
    [int]$MaxTry = 3,
    [string]$ToolsPath = "C:\tools\phpdev",
    [string]$PhpBin = "php"
)

$scriptPath = Split-Path -parent $MyInvocation.MyCommand.Definition
. "$scriptPath\logger.ps1" -ToolName "deps"

function dl{
    param (
        $Uri, $Dest
    )

    for ($i=0; $i -lt $MaxTry; $i++){
        try{
            Invoke-WebRequest -Uri $Uri -OutFile $Dest
            return 'ok'
        }catch [System.Net.WebException],[System.IO.IOException]{
            warn "Failed download ${Uri}, try again."
            continue
        }
    }
    return 'failed'
}

if($DllDeps.Length -Lt 1){
    warn "No deps specified, just skipped."
    exit 0
}

# things we used later
try{
    $phpver = & $PhpBin -r "echo PHP_MAJOR_VERSION . '.' . PHP_MINOR_VERSION . PHP_EOL;"
    $phpinfo = & $PhpBin -i
    $phpvcver = ($phpinfo | Select-String -Pattern 'PHP Extension Build => .+,(.+)' -CaseSensitive -List).Matches.Groups[1]
    $phparch = ($phpinfo | Select-String -Pattern 'Architecture => (.+)' -CaseSensitive -List).Matches.Groups[1]
}finally{
    if(
        !$phpver -Or
        !$phpvcver -Or
        !$phparch
    ){
        err "Cannot determine php attributes, do you have php in PATH?"
        warn "phpver: $phpver"
        warn "phpvcver: $phpvcver"
        warn "phparchver: $phparchver"
        exit 1
    }
}

for ($i=0; $i -lt $MaxTry; $i++){
    try{
        info "Try to fetch deps list from windows.php.net"
        $filelist = (Invoke-WebRequest `
            -Uri "https://windows.php.net/downloads/php-sdk/deps/series/packages-$phpver-$phpvcver-$phparch-staging.txt").Content
    }catch [System.Net.WebException],[System.IO.IOException]{
        warn "Failed to fetch deps list from windows.php.net"
        Write-Host $_
        continue
    }
    if($filelist){
        break
    }
}
if(!$filelist){
    err "Cannot get deps information from windows.php.net"
    exit 1
}

foreach ($depname in $DllDeps) {
    $depfile = $null
    foreach ($filename in $filelist.Split()) {
        if($filename.StartsWith($depname)){
            info "Found file $filename for dep $depname"
            $depfile = $filename
            break
        }
    }
    if(!$depfile){
        err "Cannot find dep file for $depname"
        exit 1
    }
    info "Downloading $filename from windows.php.net"
    if (-Not (Test-Path -Path $ToolsPath -PathType Container)){
        info "Creating dir" $ToolsPath
        New-Item -Path $ToolsPath -ItemType Container | Out-Null
    }
    if (-Not (Test-Path -Path $ToolsPath\deps -PathType Container)){
        info "Creating dir $ToolsPath\deps"
        New-Item -Path $ToolsPath\deps -ItemType Container | Out-Null
    }
    $dest = "$ToolsPath\deps\$depfile"
    if(Test-Path $dest -PathType Leaf){
        warn "$depfile is already provided, instant extract it."
    }else {
        $uri = "https://windows.php.net/downloads/php-sdk/deps/$phpvcver/$phparch/$depfile"
        $ret = dl -Uri $uri -Dest $dest
        if (-Not "ok".Equals($ret)){
            err "Failed download $uri."
            exit 1
        }
    }
    info "Unzipping $dest to $ToolsPath\deps\"
    Expand-Archive $dest -Destination "$ToolsPath\deps\" -Force
    info "Install dlls and pdbs into $phpexedir"
    Copy-Item -Path "$ToolsPath\deps\bin\*.dll" -Destination $phpexedir
    Copy-Item -Path "$ToolsPath\deps\bin\*.pdb" -Destination $phpexedir
}

info "Done."
