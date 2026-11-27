param([string]$Root = "src")
$bom = [byte[]](0xEF, 0xBB, 0xBF)
$changed = 0
$files = Get-ChildItem -Path $Root -Include *.c, *.h -Recurse
foreach ($f in $files) {
    $bytes = [System.IO.File]::ReadAllBytes($f.FullName)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        [System.IO.File]::WriteAllBytes($f.FullName, $bytes[3..($bytes.Length - 1)])
        $changed++
    }
}
Write-Output "[build] source encoding normalized to UTF-8 without BOM ($changed file(s) updated)"
