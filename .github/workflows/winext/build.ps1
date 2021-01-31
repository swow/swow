#php extension builder

param (
    [System.Object[]]$ExtraArgs,
    [string]$ToolsPath = "C:\tools\phpdev",
    [string]$ExtPath = ".",
    [string]$ExtName
)

$scriptPath = Split-Path -parent $MyInvocation.MyCommand.Definition
. "$scriptPath\utils.ps1" -ToolName "build" -MaxTry $MaxTry

info "Start building php extension"
$origwd = (Get-Location).Path
Set-Location $ExtPath

info "Phpize it"
phpize.bat
if (0 -Ne $lastexitcode){
    err "Failed phpize it, are we at ext dir?"
    Set-Location $origwd
    exit 1
}
$buildargs = ,".\configure.bat"
if("".Equals($ExtName)){
    warn "No ext name given, will not prepend --enable-extname arg"
}else{
    $buildargs += ,"--enable-$ExtName"
}
$buildargs += ,"--with-php-build=$ToolsPath\deps"
$buildargs += $ExtraArgs
info "Configure it with arg $buildargs"
Invoke-Expression "$buildargs"
if (0 -Ne $lastexitcode){
    err "Failed configure."
    Set-Location $origwd
    exit 1
}
info "Start nmake"
nmake
if (0 -Ne $lastexitcode){
    err "Failed nmake."
    Set-Location $origwd
    exit 1
}
Set-Location $origwd

exit 0
