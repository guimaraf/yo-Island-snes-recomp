# Executar com o jogo instrumentado já aberto. Não inicia, pausa ou injeta inputs.
[CmdletBinding()]
param(
    [int]$GameProcessId = 0,
    [Parameter(Mandatory=$true)]
    [ValidateSet('A','B')]
    [string]$Renderer,
    [Parameter(Mandatory=$true)]
    [ValidateSet('Passagem','Castelo')]
    [string]$Scenario,
    [string]$OutputRoot = ''
)
$ErrorActionPreference = 'Stop'
$ErrorId = 'ER-07'
$expectedRenderer = if ($Renderer -eq 'A') { '0' } else { '1' }
$Scene = if ($Scenario -eq 'Passagem') {
    'Mundo 1, fase 1-3; passagem secreta; movimento da barra com peso'
} else {
    'Primeiro castelo; descida de porta interativa; identificar sala e porta nas notas'
}
if ($GameProcessId -gt 0) {
    $gameProcess = Get-Process -Id $GameProcessId
} else {
    # Include both the normal executable and diagnostic names such as
    # "Yoshi's Island recomp.before-instrumentation-....". The binary marker
    # check below still rejects backups without the timed capture receiver.
    $candidates = @(Get-Process | Where-Object {
        $_.ProcessName -like "Yoshi's Island recomp*" -or
        $_.ProcessName -in @('SuperMarioWorld2YoshiSIslandSNESRecomp', 'yoshi-instrumentado')
    })
    if ($candidates.Count -ne 1) {
        if ($candidates.Count -eq 0) {
            throw "Nenhum processo Yoshi's Island recomp foi encontrado. Abra Yoshi's Island recomp.exe (a build instrumentada atual) e tente de novo."
        }
        throw "Encontrados $($candidates.Count) processos do jogo. Feche instâncias extras ou informe -GameProcessId com o PID da instância que quer capturar."
    }
    $gameProcess = $candidates[0]
}
$exePath = $gameProcess.MainModule.FileName
if (!$exePath) { throw 'Não foi possível identificar o executável do processo.' }
$exeDirectory = Split-Path $exePath -Parent
# Detectar ausência do receptor antes de criar arquivos ou aguardar Enter.
# Não identifica uma imagem já carregada diferente do arquivo atual em disco.
$exeText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($exePath))
foreach ($marker in @('yi-render-timed-window-v2','yi-render-timed-window-er05-10s-v1','yi-render-repeat-v1','yi-render-raster-probe-v1','yi-render-dma-hdma-v1','yi-render-input-v1','yi-render-ab-v1','yi-render-select-v1','yi-render-vram-consumed-v1','yi-er07-bg3vofs-defer-v1')) {
    if (!$exeText.Contains($marker)) {
        throw "O EXE não contém $marker. Abra a build instrumentada compatível. Nenhuma pasta de coleta foi criada."
    }
}
$exeText = $null
$requestPath = Join-Path $exeDirectory ("yi-capture-request-{0}.txt" -f $gameProcess.Id)
if (Test-Path -LiteralPath $requestPath) {
    throw "Já existe um pedido pendente para este jogo: $requestPath. Preserve a coleta anterior e aguarde capture.done ou capture.failed antes de tentar novamente."
}
if (!$OutputRoot) { $OutputRoot = $exeDirectory }
$outputPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputRoot)
if ([Text.Encoding]::UTF8.GetByteCount($outputPath) -gt 850 -or $outputPath -match '[\r\n]') {
    throw 'Use uma pasta de saída mais curta e sem quebras de linha.'
}
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$capturePath = Join-Path $outputPath ('yoshi-render-ER07-' + $Scenario + '-' + $Renderer + '-' + (Get-Date -Format yyyyMMdd-HHmmss) + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $capturePath | Out-Null
$manifest = [ordered]@{
    schema='yi-render-er07-ab-v3'; candidate='yi-er07-bg3vofs-defer-v1'; consumedVramProbe='yi-render-vram-consumed-v1'; receiver='yi-render-timed-window-er05-10s-v1'; rasterProbe='yi-render-raster-probe-v1'; errorId=$ErrorId; scene=$Scene;
    processId=$gameProcess.Id; processStarted=$gameProcess.StartTime.ToString('o');
    exe=$exePath; exeSHA256=(Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash;
    requestedAt=(Get-Date).ToString('o'); preparationSeconds=5; captureDurationSeconds=10; preFrames=0; frameCap=620;
    sourceBinaryCorrespondence='confirmar build local; hash do arquivo em disco não autentica a imagem já carregada';
    inputs='Enter somente no console; nenhum input injetado no jogo';
    scenario=$Scenario; requestedRenderer=$Renderer; expectedRendererValue=[int]$expectedRenderer;
    rendererSelection='receptor aplica o modo solicitado no desenho durante a coleta; flags restaurados apos cada desenho';
    configuration='anotar widescreen, escala, mods, foco e inputs em notes.txt';
    status='requested'
}
$configHashes = @{}
foreach ($name in @('config.ini','keybinds.ini')) {
    $configPath = Join-Path $exeDirectory $name
    if (Test-Path -LiteralPath $configPath) { $configHashes[$name] = (Get-FileHash -LiteralPath $configPath -Algorithm SHA256).Hash }
}
$manifest['configHashes'] = $configHashes
$repoPath = Split-Path $PSScriptRoot -Parent
$sourceHashes = @{}
foreach ($source in @('src/game_rtl.c','src/render_capture.h','modules/snesrecomp/runner/src/beam_frame_driver.c','modules/snesrecomp/runner/src/snes/ppu.c','modules/snesrecomp/runner/src/snes/ppu.h','modules/snesrecomp/runner/src/snes/dma.c','modules/snesrecomp/runner/src/snes/dma.h','modules/snesrecomp/runner/src/snes/snes.c','modules/snesrecomp/runner/src/snes/superfx.c')) {
    $sourcePath = Join-Path $repoPath $source
    if (Test-Path -LiteralPath $sourcePath) { $sourceHashes[$source] = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash }
}
$manifest['sourceHashes'] = $sourceHashes
$manifestPath = Join-Path $capturePath 'session.json'
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding utf8
$sceneInstructions = if ($Scenario -eq 'Passagem') {
    'Prepare a passagem antes de mover a barra e antes da corrupção. Dentro da janela, registre cerca de 1 segundo parado, mova a barra normalmente e continue após o evento.'
} else {
    'Prepare Yoshi antes do gatilho da porta. Dentro da janela, registre cerca de 1 segundo antes, ative a descida normalmente e continue após o evento. Identifique a sala e a porta sem presumir o número da fase.'
}
@"
ID: $ErrorId
Cena: $Scene
Renderer esperado: $Renderer ($expectedRenderer). Conferir resultado no session.json e renderer por linha no capture.tsv.
$sceneInstructions
Anotar: sala/caminho/porta, posição inicial, câmera, botões/dispositivo/mapeamento, foco/pausas, carregamento de estado, widescreen, escala/resolução, backend, filtros/mods, limites de sprites e velocidade.
Relato: o defeito apareceu? A cena começou normal? A interação e o período posterior couberam na janela?
Não incluir ROM ou dumps de memória. Preservar a pasta inteira.
"@ | Set-Content -LiteralPath (Join-Path $capturePath 'notes.txt') -Encoding utf8
# Publicação atômica: o jogo nunca deve ler um caminho parcialmente escrito.
$temporaryRequest = Join-Path $capturePath 'attach-request.txt'
$requestText = $capturePath + "`ntimed2prep5cap10`nrenderer $expectedRenderer`n"
# Reutiliza somente o protocolo temporizado de 10 s; não envia box nem ativa sondas ER-05.
[IO.File]::WriteAllText($temporaryRequest, $requestText, $utf8NoBom)
# Publique por rename no mesmo diretório/volume do destino.
$requestStaging = $requestPath + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
$requestStream = [IO.File]::Open($requestStaging, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
try {
    $requestBytes = $utf8NoBom.GetBytes($requestText)
    $requestStream.Write($requestBytes, 0, $requestBytes.Length)
} finally { $requestStream.Dispose() }
[IO.File]::Move($requestStaging, $requestPath)
Write-Host "Jogo aberto: PID $($gameProcess.Id). Saída: $capturePath"
Write-Host 'Para repetir no mesmo processo, aguarde capture.done ou capture.failed da coleta anterior antes de iniciar esta.'
Write-Host "ER-07 / $Scenario / renderer esperado $Renderer ($expectedRenderer)."
Write-Host 'O receptor seleciona automaticamente A/legado ou B/novo durante esta coleta, sem injetar teclas. Não pressione R para selecionar o modo.'
Write-Host 'Mantenha widescreen desativado nas duas coletas para comparar a mesma configuração. A coleta A será recusada se houver largura extra. O modo efetivo será validado no TSV.'
Write-Host $sceneInstructions
Write-Host 'Não espere o defeito aparecer para iniciar. Não há histórico anterior. Não alterne o renderer durante a janela.'
$armTimer = [Diagnostics.Stopwatch]::StartNew()
$armedPath = Join-Path $capturePath 'capture.armed'
while (!(Test-Path -LiteralPath $armedPath) -and !(Test-Path -LiteralPath (Join-Path $capturePath 'capture.failed')) -and !$gameProcess.HasExited -and $armTimer.Elapsed.TotalSeconds -lt 30) {
    Start-Sleep -Milliseconds 100
}
if (!(Test-Path -LiteralPath $armedPath) -or !([IO.File]::ReadAllText($armedPath).Contains('yi-er07-bg3vofs-defer-v1'))) {
    $manifest['status'] = 'arming_not_confirmed'
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding utf8
    throw "O processo não confirmou a instrumentação. Preserve a pasta: $capturePath"
}
$manifest['runtimeInputProbe'] = 'yi-er07-bg3vofs-defer-v1 confirmado pelo processo em capture.armed'
Read-Host 'Pressione Enter quando quiser capturar' | Out-Null
$captureTimer = [Diagnostics.Stopwatch]::StartNew()
$triggerUtc = [DateTimeOffset]::UtcNow
$captureStartUtc = $triggerUtc.AddSeconds(5)
$captureEndUtc = $captureStartUtc.AddSeconds(10)
$captureStartUnixMs = $captureStartUtc.ToUnixTimeMilliseconds()
$deadlineUnixMs = $captureEndUtc.ToUnixTimeMilliseconds()
if ($gameProcess.HasExited) { throw 'O jogo encerrou antes do acionamento. Os arquivos foram preservados.' }
$manifest['triggerRequestedAt'] = $triggerUtc.ToString('o')
$manifest['captureStartAt'] = $captureStartUtc.ToString('o')
$manifest['captureEndAt'] = $captureEndUtc.ToString('o')
$manifest['captureStartUnixMs'] = $captureStartUnixMs
$manifest['deadlineUnixMs'] = $deadlineUnixMs
$manifest['armedBeforeEnter'] = Test-Path -LiteralPath (Join-Path $capturePath 'capture.armed')
$manifest['status'] = 'preparing'
# Publicar o prazo por rename: o receptor nunca lê um timestamp parcial.
$triggerStaging = Join-Path $capturePath 'trigger.tmp'
$triggerStream = [IO.File]::Open($triggerStaging, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
try {
    $deadlineBytes = [Text.Encoding]::ASCII.GetBytes([string]$captureStartUnixMs + "`n" + [string]$deadlineUnixMs + "`n")
    $triggerStream.Write($deadlineBytes, 0, $deadlineBytes.Length)
} finally { $triggerStream.Dispose() }
[IO.File]::Move($triggerStaging, (Join-Path $capturePath 'trigger'))
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding utf8
Write-Host 'Preparação: 5 segundos. Volte ao jogo; quando a captura começar, execute ou mantenha a ação que deseja registrar.'
$lastSeconds = -1
while ($captureTimer.Elapsed.TotalSeconds -lt 5) {
    $secondsLeft = [int][Math]::Ceiling(5 - $captureTimer.Elapsed.TotalSeconds)
    if ($secondsLeft -ne $lastSeconds) {
        Write-Progress -Activity 'Preparação para captura' -Status "$secondsLeft segundos até começar; deixe a ação pronta"
        $lastSeconds = $secondsLeft
    }
    $remainingMs = [int][Math]::Ceiling(5000 - $captureTimer.Elapsed.TotalMilliseconds)
    if ($remainingMs -gt 0) { Start-Sleep -Milliseconds ([Math]::Min(100, $remainingMs)) }
}
Write-Progress -Activity 'Preparação para captura' -Completed
Write-Host 'Capturando por 10 segundos: registre o antes, execute a interação e mantenha o jogo avançando depois.'
$lastSeconds = -1
while ($captureTimer.Elapsed.TotalSeconds -lt 15) {
    $secondsLeft = [int][Math]::Ceiling(15 - $captureTimer.Elapsed.TotalSeconds)
    if ($secondsLeft -ne $lastSeconds) {
        Write-Progress -Activity 'Coleta ER-07 de 10 segundos' -Status "$secondsLeft segundos restantes; mantenha o jogo avançando"
        $lastSeconds = $secondsLeft
    }
    $remainingMs = [int][Math]::Ceiling(15000 - $captureTimer.Elapsed.TotalMilliseconds)
    if ($remainingMs -gt 0) { Start-Sleep -Milliseconds ([Math]::Min(100, $remainingMs)) }
}
Write-Progress -Activity 'Coleta ER-07 de 10 segundos' -Completed
$captureTimer.Stop()
$manifest['windowElapsedMs'] = $captureTimer.Elapsed.TotalMilliseconds
$manifest['preparationElapsedMs'] = [Math]::Min(5000, $captureTimer.Elapsed.TotalMilliseconds)
$manifest['captureWindowElapsedMs'] = [Math]::Max(0, $captureTimer.Elapsed.TotalMilliseconds - 5000)
$manifest['gameExitedAtWindowEnd'] = $gameProcess.HasExited
$manifest['armedAtWindowEnd'] = Test-Path -LiteralPath (Join-Path $capturePath 'capture.armed')
$donePath = Join-Path $capturePath 'capture.done'
$failedPath = Join-Path $capturePath 'capture.failed'
$manifest['status'] = 'window_ended_export_pending'
Write-Host 'Janela de captura encerrada. Aguardando a gravação dos arquivos (limite: 60 segundos).'
$exportTimer = [Diagnostics.Stopwatch]::StartNew()
$lastExportNotice = -1
while (!(Test-Path -LiteralPath $donePath) -and !(Test-Path -LiteralPath $failedPath) -and
       !$gameProcess.HasExited -and $exportTimer.Elapsed.TotalSeconds -lt 60) {
    $waitedSeconds = [int][Math]::Floor($exportTimer.Elapsed.TotalSeconds)
    if ($waitedSeconds -ge 5 -and $waitedSeconds % 5 -eq 0 -and $waitedSeconds -ne $lastExportNotice) {
        Write-Host "Exportação ainda em andamento ($waitedSeconds s). Mantenha o jogo aberto."
        $lastExportNotice = $waitedSeconds
    }
    Start-Sleep -Milliseconds 200
}
$exportTimer.Stop()
$manifest['exportWaitMs'] = $exportTimer.Elapsed.TotalMilliseconds
if (Test-Path -LiteralPath $donePath) { $manifest['status'] = 'exported_not_validated' }
elseif (Test-Path -LiteralPath $failedPath) { $manifest['status'] = 'failed' }
elseif ($gameProcess.HasExited) { $manifest['status'] = 'game_exited_before_export' }
else { $manifest['status'] = 'export_wait_timeout' }
# Não confundir modo solicitado com modo realmente consumido pela PPU.
$rendererCounts = @{ legacy=0; new=0; invalid=0 }
$frameModes = @{}
$consumedRows = 0
$bg3SampleRows = 0
$captureTsv = Join-Path $capturePath 'capture.tsv'
if ($manifest['status'] -eq 'exported_not_validated' -and (Test-Path -LiteralPath $captureTsv)) {
    foreach ($line in [IO.File]::ReadLines($captureTsv)) {
        if ($line.StartsWith("consumed_vram`t")) { $consumedRows++; continue }
        if ($line.StartsWith("bg3_sample`t")) { $bg3SampleRows++; continue }
        if (!$line.StartsWith("row`t")) { continue }
        $columns = $line.Split("`t")
        if ($columns.Count -ne 22 -or $columns[3] -ne '1' -or $columns[21] -notin @('0','1')) {
            $rendererCounts.invalid++; continue
        }
        $frameId = $columns[1]
        if (!$frameModes.ContainsKey($frameId)) { $frameModes[$frameId] = @{ legacy=0; new=0 } }
        if ($columns[21] -eq '0') { $rendererCounts.legacy++; $frameModes[$frameId].legacy++ }
        else { $rendererCounts.new++; $frameModes[$frameId].new++ }
    }
}
$manifest['consumedVramRows'] = $consumedRows
$manifest['bg3SampleRows'] = $bg3SampleRows
$manifest['consumedVramValidation'] = 'unavailable'
if ($consumedRows -gt 0 -and $consumedRows -eq $rendererCounts.legacy + $rendererCounts.new -and $bg3SampleRows -eq $consumedRows) {
    $manifest['consumedVramValidation'] = 'record_count_matches_valid_renderer_rows'
    Write-Host "Nova telemetria presente: $consumedRows linhas de VRAM e BG3."
} else {
    Write-Warning 'Nova telemetria ausente ou contagem incompatível; preserve a pasta para análise.'
}
$manifest['rendererRows'] = $rendererCounts
$manifest['rendererFrames'] = @($frameModes.GetEnumerator() | Sort-Object { [int]$_.Key } | ForEach-Object {
    [ordered]@{ frame=[int]$_.Key; legacyRows=$_.Value.legacy; newRows=$_.Value.new }
})
$manifest['rendererValidation'] = 'unavailable'
if ($rendererCounts.legacy + $rendererCounts.new -gt 0) {
    $unexpected = if ($expectedRenderer -eq '0') { $rendererCounts.new } else { $rendererCounts.legacy }
    $incompleteFrames = @($frameModes.Values | Where-Object { $_.legacy + $_.new -ne 224 }).Count
    if ($unexpected -gt 0 -or $rendererCounts.invalid -gt 0 -or $incompleteFrames -gt 0) {
        $manifest['rendererValidation'] = 'mismatch_or_invalid_rows'
        Write-Warning "Coleta incompatível com renderer $Renderer ou com linhas inválidas. A=$($rendererCounts.legacy), B=$($rendererCounts.new), inválidas=$($rendererCounts.invalid). Preserve a pasta e confira widescreen/modo antes de repetir."
    } else {
        $manifest['rendererValidation'] = 'matched_all_recorded_rows'
        Write-Host "Renderer $Renderer confirmado em todas as linhas registradas. Integridade e conteúdo da sequência ainda exigem análise."
    }
} else {
    Write-Warning 'Renderer não validado: exportação incompleta ou nenhuma linha válida. Preserve a pasta inteira.'
}
$manifest['captureDoneObserved'] = Test-Path -LiteralPath $donePath
$manifest['captureFailedObserved'] = Test-Path -LiteralPath $failedPath
$manifest['filesObservedAtExit'] = @(Get-ChildItem -LiteralPath $capturePath -File | Select-Object Name,Length)
$manifest['scriptFinishedAt'] = (Get-Date).ToString('o')
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding utf8
if ($manifest['status'] -eq 'exported_not_validated') {
    Write-Host "Exportação concluída: $capturePath"
    Write-Host 'Preserve a pasta inteira para análise; perdas e alinhamento ainda precisam ser conferidos.'
} elseif ($manifest['status'] -eq 'failed') {
    $failureDetails = Get-Content -LiteralPath $failedPath -Raw -Encoding UTF8
    Write-Host "A captura terminou com falha: $capturePath"
    Write-Host $failureDetails
    Write-Host 'Os arquivos produzidos foram preservados; verifique capture.failed e capture.tsv.'
} else {
    Write-Host "A preparação de 5 segundos e a captura de 10 segundos terminaram, mas a exportação não confirmou conclusão: $($manifest['status']). Arquivos: $capturePath"
    Write-Host 'Preserve a pasta inteira. Não feche nem encerre o jogo antes de verificar se capture.done ou capture.failed apareceu.'
}
