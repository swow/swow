# php downloader

param (
    [int]$MaxTry = 3,
    [string]$ToolsPath = "C:\tools\phpdev",
    [string]$PhpVer = "",
    [string]$PhpVCVer = "",
    [string]$PhpArch = "x64",
    [bool]$PhpTs = $false
)

$scriptPath = Split-Path -parent $MyInvocation.MyCommand.Definition
. "$scriptPath\utils.ps1" -ToolName "getphp" -MaxTry $MaxTry

$guessedVCVers = @{
    "8.0" = "VS16";
    "7.4" = "VC15";
    "7.3" = "VC15";
    "7.2" = "VC15";
}

if ("".Equals($PhpVCVer)){
    $PhpVCVer = $guessedVCVers[$PhpVer]
}

if(
    !$PhpVer -Or
    !$PhpVCVer -Or
    !$PhpArch
){
    err "Cannot determine php attributes"
    warn "phpver: $PhpVer"
    warn "phpvcver: $PhpVCVer"
    warn "phparch: $phparch"
    exit 1
}

if($PhpTs){
    $phpvar = "ts-$PhpVCVer-$PhpArch"
    $dashnts = ""
    $underscorets = "_TS"
}else{
    $phpvar = "nts-$PhpVCVer-$PhpArch"
    $dashnts = "-nts"
    $underscorets = ""
}

info "Fetching releases list for PHP $PhpVer $phpvar"
$info = fetchinfo -Uri "https://windows.php.net/downloads/releases/releases.json" | ConvertFrom-Json
if(!$info){
    warn "Cannot fetch php releases info from windows.php.net."
}

$dest = $null
if($info.$PhpVer.$phpvar){
    info "Found in releases, use it"
    $latest = $info.$PhpVer.$phpvar."zip"
    if($latest.sha1){
        $hash = $latest.sha1
        $hashmethod = "SHA1"
    }elseif($latest.sha256){
        $hash = $latest.sha256
        $hashmethod = "SHA256"
    }else{
        warn "No hash for this file provided or not supported."
    }
    $dest = "$ToolsPath\" + ($latest.path)

    if($hashmethod -And (Test-Path $dest -PathType Leaf)){
        if($hashmethod -And $hash -Eq (Get-FileHash $dest -Algorithm $Hashmethod).Hash){
            warn "$dest is already provided, skipping downloading."
        }
    }
    
    provedir $ToolsPath
    $ret = dlwithhash `
        -Uri ("https://windows.php.net/downloads/releases/" + ($latest.path)) `
        -Dest $dest `
        -Hash $hash `
        -Hashmethod $hashmethod
}else{
    info "Cannot find in releases, fetching php list from windows.php.net"
    $page = fetchinfo "https://windows.php.net/downloads/releases/archives/"
    $fn = searchfile $page -Pattern ('php-(?<ver>' + $PhpVer + '[^-]+?)' + $dashnts +'-Win32-' + $PhpVCVer + '-' + $PhpArch + '.zip')
    if(!$fn){
        warn "Cannot fetch archives list, use oldest instead"
        Write-Host $_
        $fn = 'php-' + $PhpVer + '.0' + $dashnts +'-Win32-' + $PhpVCVer + '-' + $PhpArch + '.zip'
    }
    $dest = "$ToolsPath\$fn"
    $ret = dlwithhash -Uri "https://windows.php.net/downloads/releases/archives/$fn" -Dest $dest
    if (!$ret){
        err "Cannot fetch $fn"
        exit 1
    }
}

if (!$dest){
    exit 1
}

info "Done downloading php as $dest, unzip it."
try{
    # if possible, should we use -PassThru to get file list?
    Expand-Archive $dest -Destination $ToolsPath\php -Force | Out-Host
}catch {
    err "Cannot unzip downloaded zip $dest to $ToolsPath\php, that's strange."
    Write-Host $_
    exit 1
}

info "Done unzipping php, generate phpenv.bat."

$env:Path = "$ToolsPath\php;$env:Path"
$env:BUILD_DIR = "$PhpArch\Release$underscorets"
$content="@ECHO OFF
SET PATH=$ToolsPath\php;%PATH%
%*
"
[IO.File]::WriteAllLines("$ToolsPath\phpenv.bat", $content)

info "Done getphp"

exit 0
