# php dev-pack downloader

param (
    [int]$MaxTry = 3,
    [string]$ToolsPath = "C:\tools\phpdev",
    [string]$PhpBin = "php"
)

$scriptPath = Split-Path -parent $MyInvocation.MyCommand.Definition
. "$scriptPath\logger.ps1" -ToolName "devpack"

# things we used later
try{
    $phpver = & $PhpBin -r "echo PHP_MAJOR_VERSION . '.' . PHP_MINOR_VERSION . PHP_EOL;"
    $phpinfo = & $PhpBin -i
    $phpvcver = ($phpinfo | Select-String -Pattern 'PHP Extension Build => .+,(.+)' -CaseSensitive -List).Matches.Groups[1]
    $phparch = ($phpinfo | Select-String -Pattern 'Architecture => (.+)' -CaseSensitive -List).Matches.Groups[1]
    $phpvar = & $PhpBin -r "echo (PHP_ZTS?:'n') . 'ts-$phpvcver-$phparch' . PHP_EOL;"
}finally{
    if(
        !$phpver -Or
        !$phpvcver -Or
        !$phparch -Or
        !$phpvar
    ){
        err "Cannot determine php attributes, do you have php in PATH?"
        warn "phpver: $phpver"
        warn "phpvcver: $phpvcver"
        warn "phparchver: $phparchver"
        warn "phpvar: $phpvar"
        exit 1
    }
}

function provedir {
    if (-Not (Test-Path -Path $ToolsPath -PathType Container)){
        info "Creating dir" $ToolsPath
        New-Item -Path $ToolsPath -ItemType Container | Out-Null
    }
}

function dlwithhash{
    param (
        $Uri, $Dest, $Hash, $Hashmethod
    )

    for ($i=0; $i -lt $MaxTry; $i++){
        try{
            info "Try to download ${uri}"
            Invoke-WebRequest -Uri $Uri -OutFile $Dest | Out-Null
        }catch [System.Net.WebException],[System.IO.IOException]{
            warn "Failed download ${uri}."
            Write-Host $_
            continue
        }
        
        if($Hashmethod -And -Not $Hash -Eq (Get-FileHash $Dest -Algorithm $Hashmethod).Hash ){
            warn "Bad checksum, remove file $Dest."
            Remove-Item $Dest | Out-Null
            continue
        }
        break
    }
    if ($hashmethod -And -Not $hash -Eq (Get-FileHash $dest -Algorithm $Hashmethod).Hash ){
        warn "Cannot download ${uri}: bad checksum."
        return "failed"
    }
    return "ok"
}
function fetchdevpack(){
    if ($info.$phpver.$phpvar) {
        info "Found target version on releases, try using latest devpack."
        $latest = $info.$phpver.$phpvar."devel_pack"
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
                return $dest
            }
        }
        
        provedir
        $ret = dlwithhash `
            -Uri ("https://windows.php.net/downloads/releases/" + ($latest.path)) `
            -Dest $dest `
            -Hash $hash `
            -Hashmethod $hashmethod
        if ("ok".Equals($ret)){
            return $dest
        }
    } else {
        info "Target version is not active release or failed to download releases info, try fetch instantly."
        $fn = & $PhpBin -r "echo 'php-devel-pack-' . PHP_VERSION . '-' . (PHP_ZTS?'nts-':'') . 'Win32-$phpvcver-$phparch.zip' . PHP_EOL;"
        provedir
        $ret = dlwithhash -Uri "https://windows.php.net/downloads/releases/archives/$fn" -Dest "$ToolsPath\$fn"
        if ("ok".Equals($ret)){
            return "$ToolsPath\$fn"
        }
    }
}

info "Finding devpack for PHP $phpver $phpvar"

for ($i=0; $i -lt $MaxTry; $i++){
    try{
        $info = Invoke-WebRequest -Uri "https://windows.php.net/downloads/releases/releases.json" | ConvertFrom-Json
    }catch [System.Net.WebException],[System.IO.IOException]{
        warn "Failed to fetch php releases info from windows.php.net, try again."
        Write-Host $_
        continue
    }
    #$info = Invoke-WebRequest -Uri "https://on" | ConvertFrom-Json
    if($info){
        break
    }
}
if(!$info){
    warn "Cannot fetch php releases info from windows.php.net."
}
$zipdest = fetchdevpack
if (-Not $zipdest){
    err "Failed download devpack zip."
    exit 1
}

info "Done downloading devpack, unzip it."

try{
    # if possible, should we use -PassThru to get file list?
    Expand-Archive $zipdest -Destination $ToolsPath -Force | Out-Host
}catch {
    err "Cannot unzip downloaded zip $zipdest to $ToolsPath, that's strange."
    Write-Host $_
    exit 1
}
$sa = New-Object -ComObject Shell.Application
$dirname = ($sa.NameSpace($zipdest).Items() | Select-Object -Index 0).Name

info "Done unzipping devpack, generate env.bat."

# Since setup-php only provides Release version PHP, yet we only support Release
# Maybe sometimes we can build PHP by ourself?
$phpts = & $PhpBin -r "echo PHP_ZTS?'TS':'';"
$content="
@ECHO OFF
SET BUILD_DIR=$phparch\Release$phpts
SET PATH=$ToolsPath\$dirname;%PATH%
$ToolsPath\php-sdk-binary-tools\phpsdk-starter.bat -c $phpvcver -a $phparch -t %*
"
$content | Out-File -Encoding UTF8 -FilePath $ToolsPath\env.bat

info "Done preparing dev-pack"
