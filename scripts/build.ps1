param([ValidateSet('amd64','arm64')][string]$Arch='amd64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$dist=Join-Path $root 'dist'
New-Item -ItemType Directory -Force $dist | Out-Null
$env:CGO_ENABLED='0'; $env:GOOS='windows'; $env:GOARCH=$Arch
go test ./...
go build -trimpath -ldflags '-s -w' -o (Join-Path $dist 'notegen-mcp.exe') ./cmd/notegen-mcp
Copy-Item (Join-Path $root 'config/config.example.json') (Join-Path $dist 'config.example.json') -Force
Copy-Item (Join-Path $root 'README.md') (Join-Path $dist 'README.md') -Force
Get-FileHash (Join-Path $dist 'notegen-mcp.exe') -Algorithm SHA256 | ForEach-Object { "$($_.Hash.ToLower())  notegen-mcp.exe" } | Set-Content (Join-Path $dist 'SHA256SUMS') -Encoding ascii

