
param (
    [string]$ToolName,
    [int]$MaxTry
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
    Write-Host -NoNewline -ForegroundColor White "$Msg"
    if($env:UNIX_COLOR) {
        Write-Host -NoNewline (0x1b -As [char])
        Write-Host -NoNewline "[0m"
    }
    Write-Host ""
}

function provedir {
    param ($Path)
    if (-Not (Test-Path -Path $Path -PathType Container)){
        info "Creating dir" $Path
        New-Item -Path $Path -ItemType Container | Out-Null
    }
}

function fetchpage {
    param ($Uri, $Headers=$null, $Method="GET", $Body=$null)
    for ($i=0; $i -lt $MaxTry; $i++){
        try{
            $ret = Invoke-WebRequest -Uri $Uri -UseBasicParsing -Headers $Headers -Method $Method -Body $Body
            return $ret
        }catch [System.Net.WebException],[System.IO.IOException]{
            warn "Failed to fetch page ${Uri}, try again."
            Write-Host $_
            continue
        }
    }
    return $null
}

function fetchjson {
    param ($Uri, $Headers, $Method="GET", $Body)
    $page = fetchpage -Uri $Uri -Headers $Headers -Method $Method -Body $Body
    if($page){
        try{
            return ($page | ConvertFrom-Json)
        }catch{
            warn "Failed parse page ${Uri} as json."
            Write-Host $_
        }
    }
    return $null
}

function dlwithhash{
    param (
        $Uri, $Dest, $Hash, $Hashmethod
    )

    for ($i=0; $i -lt $MaxTry; $i++){
        try{
            info "Try to download ${uri}"
            Invoke-WebRequest -Uri $Uri -OutFile $Dest -UseBasicParsing | Out-Null
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
        return $false
    }
    return $true
}

function script:vercompare{
    param ($a,$b,$size)

    for($i = 0; $i -Lt $size; $i++){
        $intvera = $a[$i].ToString() -As [int]
        $intverb = $b[$i].ToString() -As [int]
        #Write-Host "$intvera $intverb"
        if($null -Ne $intvera -And $null -Ne $intverb){
            if($intvera -Eq $intverb){
                continue
            }
            return $intvera -Gt $intverb
        }else{
            if($a[$i].ToString().Equals($b[$i].ToString())){
                continue
            }
            return $a[$i].ToString() -Gt $b[$i].ToString()
        }
    }

    return $false
}
function script:filecompare{
    param ($a, $b)
    $tuplea = ($a | Select-String -Pattern "^([^.]+)\.([^.]+)\.([^.-]+)$").Matches.Groups
    if(!$tuplea.Success){
        $tuplea = ($a | Select-String -Pattern "^([^.]+)\.([^.-]+)$").Matches.Groups
        if(!$tuplea.Success){
            $plaina = $a
        }
    }
    $tupleb = ($b | Select-String -Pattern "^([^.]+)\.([^.]+)\.([^.-]+)$").Matches.Groups
    if(!$tupleb.Success){
        $tupleb = ($b | Select-String -Pattern "^([^.]+)\.([^.-]+)$").Matches.Groups
        if(!$tupleb.Success){
            $plainb = $b
        }
    }

    if ($plaina -Or $plainb){
        #Write-Host "plain"
        if($plaina -Eq $plainb){
            # TODO: use latest (not possible come here yet)
            return $false
        }
        return $plaina -Gt $plainb
    }

    $triplea = ,$tuplea[1], $tuplea[2], (&{if($tuplea[3]) {$tuplea[3]} else {"0"}});
    $tripleb = ,$tupleb[1], $tupleb[2], (&{if($tupleb[3]) {$tupleb[3]} else {"0"}});
    #Write-Host $triplea
    #Write-Host $tripleb
    return (script:vercompare -a $triplea -b $tripleb -size 3)
}

function searchfile{
    param ($List, $Pattern)
    #"3/22/2011  1:30 PM     19378176"
    $fileinfore = "(?<mon>\d+)/(?<day>\d+)/(?<year>\d+)\s+(?<hour>\d+):(?<min>\d+)\s+(?<pmam>PM|AM)\s+(?<size>\d+)\s+"

    $match = ($List | Select-String `
        -List `
        -Pattern ($fileinfore + '<A HREF="[^"]+?">(?<fn>' + $Pattern + ')</A>') ).Matches[0]
    if(!$match){
        return $null
    }
    $used = $match.Groups

    for($match;
        $match.Success;
        $match = $match.NextMatch()){
        if(script:filecompare -a $match.Groups["ver"].ToString() -b $used['ver'].ToString()){
            $used = $match.Groups
        }
    }

    return @($used['fn'].ToString(), $used['ver'].ToString())
}