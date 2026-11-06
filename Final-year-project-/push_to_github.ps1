param(
    [string]$NewRepoName = "Smart-Glasses-HUD-v2",
    [string]$Description = "Upgraded Smart Glasses HUD (Raspberry Pi Zero W, SSD1306 OLED, WM8960 Audio TTS, Android Companion App & Step-by-Step Build Guide)"
)

# Creates a new GitHub repository and uploads the entire Smart Glasses v2.0 project via GitHub REST API.
# Reads GITHUB_TOKEN from C:\Users\sharish\.env (or prompts securely if missing) without printing it.

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
$envFile = "C:\Users\sharish\.env"

# Load GITHUB_TOKEN from C:\Users\sharish\.env if not in environment
if (-not $env:GITHUB_TOKEN) {
    if (Test-Path $envFile) {
        Get-Content $envFile | ForEach-Object {
            if ($_ -match "^GITHUB_TOKEN=(.+)$") {
                $env:GITHUB_TOKEN = $matches[1].Trim()
            }
        }
    }
}

# If still not set, prompt securely (typing hidden) and save to C:\Users\sharish\.env
if (-not $env:GITHUB_TOKEN) {
    $sec = Read-Host "Enter GITHUB_TOKEN (typing hidden)" -AsSecureString
    $val = [Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($sec))
    if ($val) {
        [System.IO.File]::AppendAllText($envFile, "GITHUB_TOKEN=$($val.Trim())`r`n")
        $env:GITHUB_TOKEN = $val.Trim()
        Write-Host "Saved GITHUB_TOKEN to $envFile"
    }
}

if (-not $env:GITHUB_TOKEN) {
    Write-Error "GITHUB_TOKEN is not set in $envFile or environment."
    exit 1
}

$Headers = @{
    "Authorization" = "Bearer $($env:GITHUB_TOKEN)"
    "Accept"        = "application/vnd.github+json"
    "User-Agent"    = "SmartGlasses-RepoCreator"
}

Write-Host "1. Identifying authenticated GitHub user..."
$user = Invoke-RestMethod -Uri "https://api.github.com/user" -Headers $Headers -Method Get
$RepoOwner = $user.login
Write-Host "   Authenticated as: $RepoOwner"

Write-Host "2. Creating new repository '$NewRepoName' under $RepoOwner..."
$createBody = @{
    name        = $NewRepoName
    description = $Description
    private     = $false
    auto_init   = $true
} | ConvertTo-Json

try {
    $repo = Invoke-RestMethod -Uri "https://api.github.com/user/repos" -Headers $Headers -Method Post -Body $createBody -ContentType "application/json"
    Write-Host "   Created repository: $($repo.html_url)"
    Start-Sleep -Seconds 2
} catch {
    Write-Host "   Repository '$NewRepoName' already exists or initialized; fetching info..."
    $repo = Invoke-RestMethod -Uri "https://api.github.com/repos/$RepoOwner/$NewRepoName" -Headers $Headers -Method Get
}

$Branch = $repo.default_branch
if (-not $Branch) { $Branch = "main" }
$BaseApi = "https://api.github.com/repos/$RepoOwner/$NewRepoName"

Write-Host "3. Fetching base commit on branch '$Branch'..."
$ref = Invoke-RestMethod -Uri "$BaseApi/git/ref/heads/$Branch" -Headers $Headers -Method Get
$latestCommitSha = $ref.object.sha
$commitObj = Invoke-RestMethod -Uri "$BaseApi/git/commits/$latestCommitSha" -Headers $Headers -Method Get
$baseTreeSha = $commitObj.tree.sha

Write-Host "4. Uploading all project files to '$RepoOwner/$NewRepoName'..."
$allFiles = Get-ChildItem -Path $ProjectRoot -Recurse -File | Where-Object {
    $_.Name -ne "push_to_github.ps1"
}

$treeItems = @()
foreach ($file in $allFiles) {
    $relPath = $file.FullName.Substring($ProjectRoot.Length).TrimStart('\', '/') -replace '\\', '/'
    Write-Host "   Uploading $relPath ($($file.Length) bytes)..."
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    $b64 = [Convert]::ToBase64String($bytes)
    $blobBody = @{
        content  = $b64
        encoding = "base64"
    } | ConvertTo-Json
    $blobResp = Invoke-RestMethod -Uri "$BaseApi/git/blobs" -Headers $Headers -Method Post -Body $blobBody -ContentType "application/json"
    $treeItems += @{
        path = $relPath
        mode = "100644"
        type = "blob"
        sha  = $blobResp.sha
    }
}

Write-Host "5. Creating Git tree and commit..."
$treeBody = @{
    base_tree = $baseTreeSha
    tree      = $treeItems
} | ConvertTo-Json -Depth 5
$newTree = Invoke-RestMethod -Uri "$BaseApi/git/trees" -Headers $Headers -Method Post -Body $treeBody -ContentType "application/json"

$commitBody = @{
    message = "Initial release of Smart Glasses HUD v2.0 (Raspberry Pi C HUD, WM8960 TTS, Android App & Step-by-Step Build Guide)"
    tree    = $newTree.sha
    parents = @($latestCommitSha)
} | ConvertTo-Json
$newCommit = Invoke-RestMethod -Uri "$BaseApi/git/commits" -Headers $Headers -Method Post -Body $commitBody -ContentType "application/json"

$refBody = @{
    sha   = $newCommit.sha
    force = $false
} | ConvertTo-Json
Invoke-RestMethod -Uri "$BaseApi/git/refs/heads/$Branch" -Headers $Headers -Method Patch -Body $refBody -ContentType "application/json" | Out-Null

Write-Host "=================================================================="
Write-Host "SUCCESS! Uploaded complete project to new GitHub repository:"
Write-Host "https://github.com/$RepoOwner/$NewRepoName"
Write-Host "=================================================================="
