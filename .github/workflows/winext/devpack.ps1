# php dev-pack downloader

param (
    [int]$MaxTry = 3,
    [string]$ToolsPath = "C:\tools\phpdev",
    [string]$PhpBin = "php",
    [string]$PhpVer = "",
    [string]$PhpVCVer = "",
    [string]$PhpArch = "x64",
    [bool]$PhpTs = $false,
    [bool]$DryRun = $false
)

$scriptPath = Split-Path -parent $MyInvocation.MyCommand.Definition
. "$scriptPath\utils.ps1" -ToolName "devpack" -MaxTry $MaxTry

if ("".Equals($PhpVer)){
    try{
        $PhpVer = & $PhpBin -r "echo PHP_MAJOR_VERSION . '.' . PHP_MINOR_VERSION . PHP_EOL;"
        $phpinfo = & $PhpBin -i
        $PhpVCVer = ($phpinfo | Select-String -Pattern 'PHP Extension Build => .+,(.+)' -CaseSensitive -List).Matches.Groups[1]
        $PhpArch = ($phpinfo | Select-String -Pattern 'Architecture => (.+)' -CaseSensitive -List).Matches.Groups[1]
        $phpvar = & $PhpBin -r "echo (PHP_ZTS?'':'n') . 'ts-$PhpVCVer-$PhpArch' . PHP_EOL;"
        $dashnts = & $PhpBin -r "echo (PHP_ZTS?'':'-nts') . PHP_EOL;"
        $underscorets = & $PhpBin -r "echo (PHP_ZTS?'_TS':'') . PHP_EOL;"
    }finally{
    }
}else{
    if($PhpTs){
        $phpvar = "ts-$PhpVCVer-$PhpArch"
        $dashnts = ""
        $underscorets = "_TS"
    }else{
        $phpvar = "nts-$PhpVCVer-$PhpArch"
        $dashnts = "-nts"
        $underscorets = ""
    }
}
if(
    !$PhpVer -Or
    !$PhpVCVer -Or
    !$PhpArch -Or
    !$phpvar
){
    err "Cannot determine php attributes, do you have php in PATH?"
    err "You can also specify vals via arguments"
    warn "phpver: $PhpVer"
    warn "phpvcver: $PhpVCVer"
    warn "phparch: $PhpArch"
    warn "phpvar: $phpvar"
    exit 1
}


function fetchdevpack(){
    if ($info.$PhpVer.$phpvar) {
        info "Found target version on releases, try using latest devpack."
        $latest = $info.$PhpVer.$phpvar."devel_pack"
        # if we are in github workflows, set ver as output
        if(${env:CI} -Eq "true"){
            Write-Host ('::set-output name=devpackver::' + $info.$PhpVer."version")
        }
        if($DryRun){
            return
        }

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

        provedir $ToolsPath
        $ret = dlwithhash `
            -Uri ("https://windows.php.net/downloads/releases/" + ($latest.path)) `
            -Dest $dest `
            -Hash $hash `
            -Hashmethod $hashmethod
        if ($ret){
            return $dest
        }
    } else {
        info "Target version is not active release or failed to download releases info, try search in file list."
        try{
            $page = fetchpage "https://windows.php.net/downloads/releases/archives/"
            $groups = ($page | Select-String `
                -List `
                -AllMatches `
                -Pattern ('<A HREF="[^"]+?">(?:php-devel-pack-' + $PhpVer + '.(?:\d+)' + $dashnts +'-Win32-' + $PhpVCVer + '-' + $PhpArch + '.zip)</A>')).Matches.Groups
            $used = 0
            foreach ($item in $groups) {
                $minor = ($item | Select-String -Pattern ($PhpVer + '.(\d+)')).Matches.Groups[1].ToString() -As "int"
                if ($minor -Gt $used){
                    $used = $minor
                }
            }
            $fn = 'php-devel-pack-' + $PhpVer + '.' + $used + $dashnts +'-Win32-' + $PhpVCVer + '-' + $PhpArch + '.zip'
            # if we are in github workflows, set ver as output
            if(${env:CI} -Eq "true"){
                Write-Host "::set-output name=devpackver::$PhpVer.$used"
            }
        }catch [System.Net.WebException],[System.IO.IOException]{
            warn "Cannot fetch archives list, use oldest instead"
            Write-Host $_
            $fn = 'php-devel-pack-' + $PhpVer + '.0' + $dashnts +'-Win32-' + $PhpVCVer + '-' + $PhpArch + '.zip'
            # if we are in github workflows, set ver as output
            if(${env:CI} -Eq "true"){
                Write-Host "::set-output name=devpackver::$PhpVer.0"
            }
        }
        if($DryRun){
            return
        }
        #Write-Host $fn
        provedir $ToolsPath
        $ret = dlwithhash -Uri "https://windows.php.net/downloads/releases/archives/$fn" -Dest "$ToolsPath\$fn"
        if ($ret){
            return "$ToolsPath\$fn"
        }
    }
}

info "Finding devpack for PHP $PhpVer $phpvar"
$info = fetchjson -Uri "https://windows.php.net/downloads/releases/releases.json"
if(!$info){
    warn "Cannot fetch php releases info from windows.php.net."
}
$zipdest = fetchdevpack
if($DryRun){
    return
}
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
$content="
@ECHO OFF
SET BUILD_DIR=$PhpArch\Release$underscorets
SET PATH=$ToolsPath\$dirname;%PATH%
SET DEVPACK_PATH=$ToolsPath\$dirname
$ToolsPath\php-sdk-binary-tools\phpsdk-starter.bat -c $PhpVCVer -a $PhpArch -t %*
"
[IO.File]::WriteAllLines("$ToolsPath\env.bat", $content)

info "Done preparing dev-pack"

exit 0
