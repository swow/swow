# php deps downloader

function dl{
    param (
        $Uri, $Dest
    )

    for ($i=0; $i -lt $env:MAX_TRY; $i++){
        try{
            Invoke-WebRequest -Uri $Uri -OutFile $Dest
            return 'ok'
        }catch [System.Net.WebException],[System.IO.IOException]{
            Write-Warning "Failed download ${Uri}, try again."
            continue
        }
    }
    return 'failed'
}

$phpver = php -r "echo PHP_MAJOR_VERSION . '.' . PHP_MINOR_VERSION . PHP_EOL;"
$phpvcver = (php -i | Select-String -Pattern 'PHP Extension Build => .+,(.+)' -CaseSensitive -List).Matches.Groups[1].ToString().ToLower() # why windows.php.net use lowercase?
$phparch = (php -i | Select-String -Pattern 'Architecture => (.+)' -CaseSensitive -List).Matches.Groups[1]
$phpexedir = ((Get-Command php.exe).Source | Select-String -Pattern '(.+)\\php.exe$' -CaseSensitive).Matches.Groups[1]

for ($i=0; $i -lt $env:MAX_TRY; $i++){
    try{
        $filelist = (Invoke-WebRequest -Uri "https://windows.php.net/downloads/php-sdk/deps/series/packages-$phpver-$phpvcver-$phparch-staging.txt").Content
    }catch [System.Net.WebException],[System.IO.IOException]{
        Write-Warning "Failed to fetch deps list from windows.php.net, try again."
        continue
    }
    if($filelist){
        break
    }
}
if(!$filelist){
    Write-Warning "Cannot get deps information from windows.php.net"
    exit 1
}

$depnames = $env:DLLDEPS.Split(",")
foreach ($depname in $depnames) {
    $depfile = $null
    foreach ($filename in $filelist.Split()) {
        if($filename.StartsWith($depname)){
            Write-Host "Found file $filename for dep $depname"
            $depfile = $filename
            break
        }
    }
    if(!$depfile){
        Write-Warning "Cannot find dep file for $depname"
        exit 1
    }
    Write-Host "Downloading $filename from windows.php.net"
    if (-Not (Test-Path -Path $env:TOOLS_PATH -PathType Container)){
        Write-Host "Creating dir" $env:TOOLS_PATH
        New-Item -Path $env:TOOLS_PATH -ItemType Container | Out-Null
    }
    if (-Not (Test-Path -Path $env:TOOLS_PATH\deps -PathType Container)){
        Write-Host "Creating dir $env:TOOLS_PATH\deps"
        New-Item -Path $env:TOOLS_PATH\deps -ItemType Container | Out-Null
    }
    $uri = "https://windows.php.net/downloads/php-sdk/deps/$phpvcver/$phparch/$depfile"
    $dest = "$env:TOOLS_PATH\deps\$depfile"
    $ret = dl -Uri $uri -Dest $dest
    if (-Not "ok".Equals($ret)){
        Write-Warning "Failed download $uri."
        exit 1
    }
    Write-Host "Unzip it."
    Expand-Archive $dest -Destination "$env:TOOLS_PATH\deps\" -Force
    Write-Host "Install dlls and pdbs into $phpexedir"
    Copy-Item -Path "$env:TOOLS_PATH\deps\bin\*.dll" -Destination $phpexedir
    Copy-Item -Path "$env:TOOLS_PATH\deps\bin\*.pdb" -Destination $phpexedir
}