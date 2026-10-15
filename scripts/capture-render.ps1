# Executar com o jogo instrumentado já aberto. Não inicia, pausa ou injeta inputs.
[CmdletBinding()]
param(
    [int]$GameProcessId = 0,
    [ValidateSet('ER-01','ER-02','ER-03','ER-04','ER-05','ER-06','controle')]
    [string]$ErrorId = 'ER-06',
    [string]$Scene = 'selecao de fases depois de concluir uma fase',
    [string]$OutputRoot = '',
    # Native ROI: x,y,width,height; required by the current ER-04/ER-05 motion probe request.
    [int[]]$BoxRegion = $null
)
$ErrorActionPreference = 'Stop'
$motionEnabled = $ErrorId -in @('ER-04','ER-05')
$boxEnabled = $ErrorId -eq 'ER-05'
if ($motionEnabled) {
    if (!$PSBoundParameters.ContainsKey('Scene')) {
        if ($boxEnabled) { $Scene = 'ER-05; identificar mensagem, fase da abertura, inputs e estado da caixa de texto em notes.txt' }
        else { $Scene = 'ER-04; identificar objeto interativo, fase, câmera, inputs e contato em notes.txt' }
    }
    if (!$PSBoundParameters.ContainsKey('BoxRegion')) { $BoxRegion = @(0,0,256,224) }
    if ($BoxRegion.Count -ne 4 -or $BoxRegion[0] -lt 0 -or $BoxRegion[1] -lt 0 -or
        $BoxRegion[2] -le 0 -or $BoxRegion[3] -le 0 -or
        $BoxRegion[0] -ge 800 -or $BoxRegion[1] -ge 224 -or
        [long]$BoxRegion[0]+$BoxRegion[2] -gt 800 -or [long]$BoxRegion[1]+$BoxRegion[3] -gt 224) {
        throw 'BoxRegion deve ser x,y,largura,altura em pixels nativos, dentro de 800x224 e com dimensões positivas.'
    }
} elseif ($PSBoundParameters.ContainsKey('BoxRegion')) {
    throw 'BoxRegion requer -ErrorId ER-04 ou ER-05.'
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
if (!$exeText.Contains('yi-render-timed-window-v2') -or !$exeText.Contains('yi-render-repeat-v1') -or !$exeText.Contains('yi-render-raster-probe-v1')) {
    throw "O executável aberto não tem o receptor atualizado com sondas do raster journal: $exePath. Faça seu build local atualizado e abra esse EXE. Copiar apenas o script não habilita a captura. Nenhuma pasta de coleta foi criada."
}
if ($boxEnabled -and !$exeText.Contains('yi-render-box-v1')) {
    throw 'Esta build ainda não contém as medidas horizontais do ER-05 (yi-render-box-v1). Compile os fontes atualizados antes de testar. Nenhuma pasta foi criada.'
}
if ($motionEnabled -and !$exeText.Contains('yi-render-motion-v1')) {
    throw 'Esta build ainda não contém as sondas de objetos e câmera (yi-render-motion-v1). Abra o executável instrumentado atualizado. Nenhuma pasta foi criada.'
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
$capturePath = Join-Path $outputPath ('yoshi-render-' + (Get-Date -Format yyyyMMdd-HHmmss) + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $capturePath | Out-Null
$manifest = [ordered]@{
    schema='yi-render-timed-window-v2'; rasterProbe='yi-render-raster-probe-v1'; errorId=$ErrorId; scene=$Scene;
    processId=$gameProcess.Id; processStarted=$gameProcess.StartTime.ToString('o');
    exe=$exePath; exeSHA256=(Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash;
    requestedAt=(Get-Date).ToString('o'); preparationSeconds=5; captureDurationSeconds=2; preFrames=0; frameCap=360;
    sourceBinaryCorrespondence='confirmar build local; hash do arquivo em disco não autentica a imagem já carregada';
    inputs='Enter somente no console; nenhum input injetado no jogo';
    configuration='anotar renderer, escala, mods, foco e inputs em notes.txt';
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
foreach ($source in @('src/game_rtl.c','src/render_capture.h','modules/snesrecomp/runner/src/beam_frame_driver.c','modules/snesrecomp/runner/src/snes/ppu.c','modules/snesrecomp/runner/src/snes/ppu.h','modules/snesrecomp/runner/src/snes/dma.c','modules/snesrecomp/runner/src/snes/superfx.c')) {
    $sourcePath = Join-Path $repoPath $source
    if (Test-Path -LiteralPath $sourcePath) { $sourceHashes[$source] = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash }
}
$manifest['sourceHashes'] = $sourceHashes
$manifest['boxProbe'] = if ($motionEnabled) { 'yi-render-box-v1' } else { $null }
$manifest['motionProbe'] = if ($motionEnabled) { 'yi-render-motion-v1' } else { $null }
$manifest['boxRegion'] = if ($motionEnabled) { @($BoxRegion) } else { $null }
$manifest['roiProbeRole'] = if ($boxEnabled) { 'ER-05 text-box analysis' } elseif ($motionEnabled) { 'auxiliary ROI required by current receiver to enable ER-04 motion probes; not a text-box test' } else { $null }
$manifestPath = Join-Path $capturePath 'session.json'
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding utf8
$sceneInstructions = if ($ErrorId -eq 'ER-04') {
    'Identifique o objeto interativo (inimigo parado ou bola), fase, direção da câmera, inputs, contato e mudanças de foco. Compare objeto e cenário; os slots e medidas não identificam automaticamente objetos. Separe coletas com câmera movendo sem contato, câmera parada e contato intencional.'
} elseif ($boxEnabled) {
    'Identifique a mensagem e a fase da abertura da caixa de texto, os inputs e mudanças de foco. Registre a sequência completa de aparecimento, desaparecimento e expansão. Não misture objetos do cenário ou inimigos com a caixa de texto.'
} else {
    'Descreva fase concluída, inputs, primeira seleção antes da fase e seleção após retorno. Registre mudanças de foco e se o gameplay permanece sem piscadas.'
}
@"
ID: $ErrorId
Cena: $Scene
$sceneInstructions
Não incluir ROM ou dumps de memória. Preservar a pasta inteira.
"@ | Set-Content -LiteralPath (Join-Path $capturePath 'notes.txt') -Encoding utf8
# Publicação atômica: o jogo nunca deve ler um caminho parcialmente escrito.
$temporaryRequest = Join-Path $capturePath 'attach-request.txt'
$requestText = $capturePath + "`ntimed2prep5`n"
# O receptor atual ativa motion_probe e roi_probe juntos ao receber "box".
# ER-04 envia a região auxiliar para habilitar motion sem classificar a cena como ER-05.
if ($motionEnabled) { $requestText += ('box {0} {1} {2} {3}' -f $BoxRegion[0],$BoxRegion[1],$BoxRegion[2],$BoxRegion[3]) + "`n" }
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
Write-Host 'Volte ao jogo e deixe a nova pasta ser armada; depois aguarde o defeito desejado.'
if ($ErrorId -eq 'ER-04') {
    Write-Host "ER-04: sondas de movimento ativas; ROI auxiliar nativa = $($BoxRegion -join ',')."
    Write-Host 'Faça coletas separadas com câmera movendo sem contato, câmera parada, contato intencional e inimigo parado. Identifique o objeto e anote a cena; slots não identificam objetos automaticamente.'
} elseif ($boxEnabled) {
    Write-Host "ER-05: sondas da abertura da caixa de texto ativas; região nativa x,y,largura,altura = $($BoxRegion -join ',')."
    Write-Host 'Prepare a cena antes da abertura. Não há histórico anterior; capture do primeiro aparecimento até a expansão completa.'
} else {
    Write-Host 'Quando o defeito aparecer, volte a este console e pressione Enter.'
}
Read-Host 'Pressione Enter quando quiser capturar' | Out-Null
$captureTimer = [Diagnostics.Stopwatch]::StartNew()
$triggerUtc = [DateTimeOffset]::UtcNow
$captureStartUtc = $triggerUtc.AddSeconds(5)
$captureEndUtc = $captureStartUtc.AddSeconds(2)
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
Write-Host 'Capturando por 2 segundos. Execute ou mantenha a ação agora.'
$lastSeconds = -1
while ($captureTimer.Elapsed.TotalSeconds -lt 7) {
    $secondsLeft = [int][Math]::Ceiling(7 - $captureTimer.Elapsed.TotalSeconds)
    if ($secondsLeft -ne $lastSeconds) {
        Write-Progress -Activity 'Coleta de 2 segundos' -Status "$secondsLeft segundos restantes; mantenha o jogo avançando"
        $lastSeconds = $secondsLeft
    }
    $remainingMs = [int][Math]::Ceiling(7000 - $captureTimer.Elapsed.TotalMilliseconds)
    if ($remainingMs -gt 0) { Start-Sleep -Milliseconds ([Math]::Min(100, $remainingMs)) }
}
Write-Progress -Activity 'Coleta de 2 segundos' -Completed
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
    Write-Host "A preparação de 5 segundos e a captura de 2 segundos terminaram, mas a exportação não confirmou conclusão: $($manifest['status']). Arquivos: $capturePath"
    Write-Host 'Preserve a pasta inteira. Não feche nem encerre o jogo antes de verificar se capture.done ou capture.failed apareceu.'
}
