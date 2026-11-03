# Downloads the SNAP wiki-topcats graph into data/ and decompresses it.
# Usage (from the repo root): powershell -ExecutionPolicy Bypass -File scripts/download_data.ps1

$ErrorActionPreference = 'Stop'

$baseUrl = 'https://snap.stanford.edu/data'
$files = @('wiki-topcats.txt', 'wiki-topcats-page-names.txt')
$dataDir = Join-Path (Split-Path -Parent $PSScriptRoot) 'data'

New-Item -ItemType Directory -Force -Path $dataDir | Out-Null

foreach ($name in $files) {
    $txtPath = Join-Path $dataDir $name
    $gzPath = "$txtPath.gz"

    if (Test-Path $txtPath) {
        Write-Host "$name already exists, skipping"
        continue
    }

    if (-not (Test-Path $gzPath)) {
        Write-Host "Downloading $name.gz"
        & curl.exe -L --fail -o $gzPath "$baseUrl/$name.gz"
        if ($LASTEXITCODE -ne 0) { throw "Download failed: $name.gz" }
    }

    Write-Host "Decompressing $name.gz"
    $in = [System.IO.File]::OpenRead($gzPath)
    $out = [System.IO.File]::Create($txtPath)
    try {
        $gzip = New-Object System.IO.Compression.GZipStream($in, [System.IO.Compression.CompressionMode]::Decompress)
        $gzip.CopyTo($out)
    }
    finally {
        if ($gzip) { $gzip.Dispose() }
        $out.Dispose()
        $in.Dispose()
    }
    Remove-Item $gzPath
}

Write-Host "Data is ready in $dataDir"
