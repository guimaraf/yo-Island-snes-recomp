param(
    [Parameter(Mandatory=$true)][string]$Exe,
    [ValidateSet('ER-01','ER-02','ER-03','ER-04','ER-05','controle')][string]$ErrorId = 'ER-03',
    [uint32]$At = 0,
    [string]$Scene = 'descrever cena e inputs',
    [string]$OutputRoot = 'G:\x2'
)
$ErrorActionPreference = 'Stop'
$exePath = (Resolve-Path -LiteralPath $Exe).Path
if ($At -gt 10000000) { throw 'At deve ser no máximo 10000000.' }
$capturePath = Join-Path $OutputRoot ('yoshi-render-' + (Get-Date -Format yyyyMMdd-HHmmss) + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $capturePath | Out-Null
$repoPath = Split-Path $PSScriptRoot -Parent
$sources = @('src/game_rtl.c','src/render_capture.h','modules/snesrecomp/runner/src/beam_frame_driver.c','modules/snesrecomp/runner/src/snes/ppu.c','modules/snesrecomp/runner/src/snes/dma.c','modules/snesrecomp/runner/src/snes/cart.c','modules/snesrecomp/runner/src/snes/superfx.c')
$hashes = @{}
foreach ($source in $sources) { $hashes[$source] = (Get-FileHash -LiteralPath (Join-Path $repoPath $source) -Algorithm SHA256).Hash }
$configHashes = @{}
foreach ($name in @('config.ini','keybinds.ini')) {
    $configPath = Join-Path (Split-Path $exePath -Parent) $name
    if (Test-Path -LiteralPath $configPath) { $configHashes[$name] = (Get-FileHash -LiteralPath $configPath -Algorithm SHA256).Hash }
}
$manifest = [ordered]@{schema='yi-render-session-v1'; errorId=$ErrorId; scene=$Scene;
    exe=$exePath; exeSHA256=(Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash;
    started=(Get-Date).ToString('o'); scheduledPortFrame=$At; sourceHashes=$hashes;
    configHashes=$configHashes; sourceBinaryCorrespondence='user must confirm local build uses these sources';
    settings='record renderer, scale, fullscreen, frameskip, turbo, mods, focus changes and input sequence in notes.txt';
    reference='Bnes comparison pending equivalent version/configuration/animation stage';
    export='native PPM and metadata only; no guest memory dumps'}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $capturePath 'session.json') -Encoding utf8
$saved = @{}
foreach ($name in @('YI_CAPTURE_DIR','YI_CAPTURE_AT','YI_TRACE_FILE')) { $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
try {
    $env:YI_CAPTURE_DIR = $capturePath
    $env:YI_CAPTURE_AT = [string]$At
    # Avoid synchronous legacy trace I/O while measuring the circular capture.
    Remove-Item Env:YI_TRACE_FILE -ErrorAction SilentlyContinue
    $process = Start-Process -FilePath $exePath -WorkingDirectory (Split-Path $exePath -Parent) -PassThru
    Write-Host "Captura: $capturePath"
    if ($At -eq 0) {
        Read-Host 'Na cena desejada, volte a este console e pressione Enter (há apenas 8 frames de histórico)' | Out-Null
        if (!$process.HasExited) { New-Item -ItemType File -Path (Join-Path $capturePath 'trigger') | Out-Null }
    }
    $process.WaitForExit()
    $manifest['finished'] = (Get-Date).ToString('o')
    $manifest['exeSHA256After'] = (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $capturePath 'session.json') -Encoding utf8
} finally {
    foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
}
