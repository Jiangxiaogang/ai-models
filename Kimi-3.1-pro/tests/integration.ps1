$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$temp = Join-Path $root 'build\verification'
$www = Join-Path $temp 'www'
$payload = Join-Path $temp 'payload.bin'
$download = Join-Path $temp 'download.bin'
$index = Join-Path $temp 'index.html'
$config = Join-Path $temp 'server.ini'
$stdout = Join-Path $temp 'stdout.txt'
$stderr = Join-Path $temp 'stderr.txt'
$port = 18080
$base = "http://127.0.0.1:$port"

function Assert($condition, $message) {
    if (!$condition) {
        throw $message
    }
}

function Assert-Curl {
    if ($LASTEXITCODE -ne 0) {
        throw "curl failed with exit code $LASTEXITCODE"
    }
}

if (Test-Path $temp) {
    Remove-Item $temp -Recurse -Force
}
New-Item $www -ItemType Directory -Force | Out-Null
New-Item (Join-Path $www 'docs') -ItemType Directory -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $www 'seed.txt'), 'seed-content', (New-Object Text.UTF8Encoding($false)))
[IO.File]::WriteAllText((Join-Path $www 'docs\hello.txt'), 'hello', (New-Object Text.UTF8Encoding($false)))
[IO.File]::WriteAllText($config, "port=$port`nroot=www`n", [Text.Encoding]::ASCII)
$bytes = New-Object byte[] (2MB + 137)
$random = [Security.Cryptography.RandomNumberGenerator]::Create()
$random.GetBytes($bytes)
$random.Dispose()
[IO.File]::WriteAllBytes($payload, $bytes)
$process = Start-Process (Join-Path $root 'httpserver.exe') -ArgumentList $config -WorkingDirectory $root -RedirectStandardOutput $stdout -RedirectStandardError $stderr -WindowStyle Hidden -PassThru
try {
    $ready = $false
    $deadline = [DateTime]::Now.AddSeconds(10)
    while ([DateTime]::Now -lt $deadline -and !$ready) {
        try {
            $client = New-Object Net.Sockets.TcpClient
            $client.Connect('127.0.0.1', $port)
            $client.Close()
            $ready = $true
        } catch {
            Start-Sleep -Milliseconds 100
        }
    }
    Assert $ready 'Server did not start'
    & curl.exe -sS --fail "$base/" -o $index
    Assert-Curl
    $html = [IO.File]::ReadAllText($index)
    Assert ($html.Contains('<title>Index of /</title>')) 'Index title is missing'
    Assert ($html.Contains('seed.txt')) 'Index listing is missing files'
    Assert ($html.Contains('x.upload.onprogress')) 'Upload progress handler is missing'
    Assert ($html.Contains('units(e.loaded/elapsed)')) 'Upload speed handler is missing'
    & curl.exe -sS --fail -X PUT -H 'Expect: 100-continue' --data-binary "@$payload" "$base/payload.bin" -o (Join-Path $temp 'put.txt')
    Assert-Curl
    & curl.exe -sS --fail "$base/payload.bin" -o $download
    Assert-Curl
    Assert ((Get-FileHash $payload).Hash -eq (Get-FileHash (Join-Path $www 'payload.bin')).Hash) 'Uploaded file differs'
    Assert ((Get-FileHash $payload).Hash -eq (Get-FileHash $download).Hash) 'Downloaded file differs'
    & curl.exe -sS --fail "$base/docs/" -o $index
    Assert-Curl
    Assert ([IO.File]::ReadAllText($index).Contains('hello.txt')) 'Subdirectory index failed'
    $redirect = & curl.exe -sS -o NUL -w '%{http_code}' "$base/docs"
    Assert-Curl
    Assert ($redirect -eq '301') 'Directory redirect failed'
    & curl.exe -sS --fail -X PUT --data-binary "@$payload" "$base/docs/a%26b.bin" -o NUL
    Assert-Curl
    & curl.exe -sS --fail "$base/docs/a%26b.bin?download=1" -o $download
    Assert-Curl
    Assert ((Get-FileHash $payload).Hash -eq (Get-FileHash $download).Hash) 'Encoded path transfer failed'
    [IO.File]::WriteAllText((Join-Path $temp 'empty.bin'), [string]::Empty)
    & curl.exe -sS --fail -X PUT --data-binary "@$(Join-Path $temp 'empty.bin')" "$base/empty.bin" -o NUL
    Assert-Curl
    Assert ((Get-Item (Join-Path $www 'empty.bin')).Length -eq 0) 'Empty upload failed'
    [IO.File]::WriteAllText((Join-Path $temp 'update.bin'), 'updated-content', [Text.Encoding]::ASCII)
    & curl.exe -sS --fail -X PUT --data-binary "@$(Join-Path $temp 'update.bin')" "$base/payload.bin" -o NUL
    Assert-Curl
    & curl.exe -sS --fail "$base/payload.bin" -o $download
    Assert-Curl
    Assert ([IO.File]::ReadAllText($download) -eq 'updated-content') 'Overwrite upload failed'
    $missing = & curl.exe -sS -o NUL -w '%{http_code}' "$base/missing.bin"
    Assert-Curl
    Assert ($missing -eq '404') 'Missing file did not return 404'
    $traversal = & curl.exe -sS --path-as-is -o NUL -w '%{http_code}' "$base/../outside.txt"
    Assert-Curl
    Assert ($traversal -eq '400') 'Path traversal was not rejected'
    $encodedTraversal = & curl.exe -sS --path-as-is -o NUL -w '%{http_code}' "$base/%2e%2e/outside.txt"
    Assert-Curl
    Assert ($encodedTraversal -eq '400') 'Encoded path traversal was not rejected'
    Write-Output 'PASS: startup, configurable port/root, autoindex, progress/speed page, binary PUT/GET, overwrite, empty file, subdirectories, URL encoding, 404, traversal rejection'
} finally {
    if (!$process.HasExited) {
        Stop-Process -Id $process.Id -Force
    }
}
