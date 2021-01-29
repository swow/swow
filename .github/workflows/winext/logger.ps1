
param (
    [string]$ToolName
)
function err{
    param ( $Msg )
    script:log -Msg $Msg -Tag "ERR" -ColorName "Red" -ColorCode "31"
}
function warn{
    param ( $Msg )
    script:log -Msg $Msg -Tag "WRN" -ColorName "Yellow" -ColorCode "33"
}
function info{
    param ( $Msg )
    script:log -Msg $Msg -Tag "IFO" -ColorName "Green" -ColorCode "32"
}
function script:log{
    param ( $Msg, $Tag, $ColorName, $ColorCode )
    if($env:UNIX_COLOR) {
        Write-Host -NoNewline (0x1b -As [char])
        Write-Host -NoNewline "[$ColorCode;1m"
    }
    Write-Host -NoNewline -ForegroundColor $ColorName "[${ToolName}:${Tag}] "
    if($env:UNIX_COLOR) {
        Write-Host -NoNewline (0x1b -As [char])
        Write-Host -NoNewline "[0;1m"
    }
    Write-Host -ForegroundColor White "$Msg"
    if($env:UNIX_COLOR) {
        Write-Host -NoNewline (0x1b -As [char])
        Write-Host -NoNewline "[0m"
    }
}