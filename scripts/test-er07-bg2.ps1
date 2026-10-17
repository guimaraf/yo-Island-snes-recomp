# Standalone, ROM-free diagnostic. Does not build or launch the game.
[CmdletBinding()]
param([string]$Gcc = 'gcc', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$frameworkRoot = Join-Path $projectRoot 'modules\snesrecomp'
if (!$OutputDirectory) { $OutputDirectory = Join-Path $projectRoot 'build-ninja\er07-bg2-test' }
$compiler = (Get-Command $Gcc -ErrorAction Stop).Source
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$exe = Join-Path $OutputDirectory 'ppu_bg2_boundary_test.exe'
$compileLog = Join-Path $OutputDirectory 'compile.log'
$compileArgs = @('-std=c11','-O1','-DSNESRECOMP_REVERSE_DEBUG=0',
    "-I$frameworkRoot\runner\src", "-I$frameworkRoot\runner\src\snes",
    "$frameworkRoot\tests\ppu\ppu_bg2_boundary_test.c",
    "$frameworkRoot\runner\src\snes\ppu.c",
    "$frameworkRoot\runner\src\snes\ppu_legacy.c", '-o', $exe)
& $compiler @compileArgs 2> $compileLog
if ($LASTEXITCODE -ne 0) { throw "Falha na compilacao do diagnostico: $compileLog" }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Diagnostico BG2 falhou.' }
