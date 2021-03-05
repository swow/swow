# dll release uploader

param (
    [string]$fn,
    [string]$Name,
    [int]$MaxTry=3,
    [string]$RelId,
    [string]$Repo,
    [string]$RunName,
    [string]$RunID,
    [string]$TestResult,
    [string]$Token
)

$scriptPath = Split-Path -parent $MyInvocation.MyCommand.Definition
. "$scriptPath\utils.ps1" -ToolName "uploader" -MaxTry $MaxTry

# gh api headers
$headers = @{
    "accept"="application/vnd.github.v3+json";
    "content-type"="application/json";
    "authorization"="Bearer ${Token}";
}

if(-Not $Name){
    err "Needs file name and id to work."
    exit 1
}
if(-Not $Repo -Or -Not $fn -Or -Not $Token -Or -Not $RelId){
    err "Needs repo, filename, release id and gh token to work."
    exit 1
}
if(-Not $RunName -Or -Not $RunID -Or -Not $TestResult){
    err "Needs run name and id to work."
    exit 1
}

function upload{
    param([string]$Name)
    info "Uploading file"
    $match = $info."upload_url" | Select-String -Pattern "(?<url>(?:http|https)://.+)(?<arg>\{.+\})"
    $upload_url = ($match.Matches[0].Groups["url"]).ToString() + "?name=$Name"
    info "URL: $upload_url"

    $form = @{
        avatar = Get-Item -Path $fn
    }

    $ret = Invoke-WebRequest `
        -Uri $upload_url `
        -Method "POST" `
        -ContentType "application/zip" `
        -Headers $headers `
        -InFile $fn
    return $ret
}

function finduri{
    $ret = fetchjson `
        -Headers $headers `
        -Uri "https://api.github.com/repos/$Repo/actions/runs/$RunID/jobs"
    #Write-Host $ret.jobs
    foreach($job in $ret.jobs) {
        #Write-Host $job.name
        #Write-Host $job.html_url
        if($job.name.ToString().Contains($RunName)){
            return $job.html_url
        }
    }
}

function patchnote{
    param([string]$Name)
    info "Fetching original notes"

    $note = $info.body.ToString()

    $mark = '<!-- mark for notes -->'

    if(!($note | Select-String $mark).Matches.Success){
        info "Add hashes markdown tag and init it's content"
        $note += "`n## Hashes and notes`n`n" + `
            "| File name | Size (in bytes) | SHA256 sum | Build log | Tests result |`n" + `
            "| - | - | - | - | - |`n" + `
            "${mark}`n"
    }
    $filesize = (Get-Item $fn).Length
    $filehash = (Get-FileHash -Algorithm SHA256 $fn).Hash
    $link = finduri
    $note = $note.Replace($mark, "| ${Name} | ${filesize} | ${filehash} | [link](${link}) | ${TestResult} |`n" + $mark)

    $patch = @{
      "body"="$note";
    } | ConvertTo-Json -Compress
    info "Repost note"
    $ret = fetchjson `
        -Body $patch `
        -Method "PATCH" `
        -Uri "https://api.github.com/repos/$Repo/releases/$RelID" `
        -Headers $headers
    return $ret
}

$info = fetchjson `
    -Uri "https://api.github.com/repos/$Repo/releases/$RelID" `
    -Headers $headers
if(!$info){
    err "Failed fetch release"
    return
}

$ret = upload -Name $Name
if (-Not $ret){
    err "Failed upload asset"
    exit 1
}

$ret = patchnote -Name $Name
if (-Not $ret){
    err "Failed patch notes"
    exit 1
}

info Done

