# Coleta delimitada das piscadas

## Jogo já aberto: 5 segundos após Enter

Depois do **seu build local atualizado com `yi-render-timed-v1`**, abra o jogo
normalmente. Copie `scripts/capture-render.ps1` para `G:\x2\scripts\`, abra
PowerShell em `G:\x2` e execute:

```powershell
.\scripts\capture-render.ps1 -ErrorId ER-06
```

Cada execução do script cria uma pasta nova com data/hora e um identificador,
inclusive ao repetir no mesmo processo. Depois que a exportação anterior fecha,
o port reutiliza o ring alocado e aceita a próxima coleta. As pastas anteriores
não são sobrescritas.

O script prepara a pasta, solicita a ativação e espera Enter no console. Volte
ao jogo e chegue à cena desejada. Ao pressionar Enter no console, começa uma
janela fixa de **5 segundos de relógio**, com contagem regressiva. Volte ao jogo
imediatamente. O script salva seu manifest e encerra ao fim da janela, sem esperar
indefinidamente pelo jogo/disco. Não inicia outra instância, não encerra o jogo,
não pausa e não envia Enter ao guest.

O jogo retém apenas frames completos obtidos após Enter e antes do prazo; não há
histórico anterior neste modo. Uma thread independente encerra a janela mesmo
se o guest parar de avançar e exporta os frames já publicados. O limite é de
360 frames e 512 MiB de pré-alocação (tamanho real em stderr/TSV). A 60 Hz são
esperados aproximadamente 300 frames se o jogo avançar durante toda a janela.
Pausa por perda de foco reduz o material; uma captura de 5 s não garante registrar
o defeito. Não use turbo/frameskip, rewind, save/load, run-ahead ou netplay.

A exportação pode terminar **depois** de o script encerrar. Aguarde `capture.done`
ou `capture.failed` antes de fechar o jogo. O script registra o estado observado
aos 5 s, não promete fechamento do disco nesse prazo. Encerrar o processo do
jogo pode interromper a gravação. A memória do lote temporizado é mantida e
reutilizada pelo mesmo processo para não liberar um slot que o produtor ainda
possa estar usando. Rode o script novamente após o fechamento da exportação para
uma nova pasta. Reset fecha antecipadamente o lote parcial.

As pastas ficam ao lado do EXE aberto, por exemplo
`G:\x2\yoshi-render-DATA-HORA-IDUNICO\`. Use `-OutputRoot` para outro destino,
inclusive acentuado. Preserve `session.json`, `notes.txt`, `capture.tsv`, todos
os `native-*.ppm` e marcadores. Não inclua ROM ou dumps de memória guest.
O script é portátil e não depende da pasta de build; hashes de fontes são
opcionais. Se houver mais de um processo/nome diferente, informe o PID real:

```powershell
.\scripts\capture-render.ps1 -GameProcessId 1234 -ErrorId ER-03 -Scene 'titulo'
```

O receptor é identificado no EXE em disco **antes de criar a pasta**; uma versão
antiga que só aceitava uma coleta por processo não é aceita para repetição no
mesmo PID. São exigidos `yi-render-timed-v1` e `yi-render-repeat-v1`. Não substitua o EXE
enquanto ele estiver aberto, pois o hash em disco não autentica a imagem carregada.
As opções anteriores `-WaitForExport` e `-TimeoutSeconds` foram removidas deste
script: a duração agora é sempre 5 segundos após Enter.

O pedido `yi-capture-request-PID.txt` fica ao lado do EXE, sem substituir arquivos
existentes (pasta gravável necessária). Ao consumir o pedido, o jogo o remove para
permitir outro teste no mesmo PID. Sua segunda linha é `timed5`. O `trigger`
contém o prazo UTC em milissegundos Unix, publicado por rename. O receptor converte
o tempo restante para seu contador monotônico; não prolonga a captura para
compensar ativação tardia. Se só receber o pedido depois do prazo, exporta uma
coleta sem frames, marcada como falha. Sem resposta do jogo, o script preserva
metadados e encerra com estado de ausência de confirmação.

`capture.tsv` v2 identifica contadores, intervalo de cada frame no clock host,
frequência desse clock, prazo UTC, frames retidos e descartes por limite. Os
slots são publicados por atomics antes de a thread exportadora lê-los; o frame
incompleto no prazo é excluído. Limites por frame continuam 8192 eventos PPU e
512 DMA, com contadores próprios. Zero frames não é sucesso. `capture.done`
confirma arquivos fechados, não ausência de perdas ou defeitos. Não se observa
o framebuffer final GPU; relógios guest e conteúdo PPU não são modificados.

O `.ps1` usa UTF-8 **com BOM** (Windows PowerShell 5.1), enquanto o protocolo de
caminhos usa UTF-8 **sem BOM**, convertido para Unicode no receptor Windows.
Preserve a codificação ao copiar. Caso o processo aberto já use trace legado,
seu custo de I/O continuará presente.

## Captura circular ER-01 a ER-05 (nova, experimental)

Implementada nos fontes, **ainda sem build ou teste de runtime**. Depois do seu
build local, use o executável resultante. O EXE antigo em G:\x2 não recebe a
instrumentação por ter o mesmo nome. Não sobrescreva sua referência antiga.

Exemplo manual, em PowerShell (ajuste o caminho do EXE recém-compilado):

```powershell
& "F:\GitRevised\SnesRecompProject\Single Player\Yoshi's Island recomp\tools\render-capture.ps1" -Exe "G:\x2\yoshi-instrumentado.exe" -ErrorId ER-03 -Scene 'titulo; anotar inputs'
```

O script inicia o jogo somente quando você o executar. Na cena, volte ao console
e pressione Enter. Essa tecla pertence ao console; nenhuma tecla global nem
atalho do jogo é interceptado. O arquivo `trigger` marca o próximo frame do port.
Trocar o foco e reagir pode demorar mais que os 8 frames anteriores disponíveis:
registre a mudança de foco e prefira o modo programado para um evento breve.
Alternativa sem trocar foco: outro console/processo pode criar `trigger` na pasta
informada. Não crie o marcador antes da cena, pois ele não é consumido/removido.

Para sequência reproduzível, após identificar o frame aproximado no mesmo fluxo
desde boot, marque uma janela programada (exemplo ilustrativo, não frame validado):

```powershell
& "F:\GitRevised\SnesRecompProject\Single Player\Yoshi's Island recomp\tools\render-capture.ps1" -Exe "G:\x2\yoshi-instrumentado.exe" -ErrorId ER-01 -At 650 -Scene 'intro sem input; intervalo a confirmar'
```

Cada sessão cria `G:\x2\yoshi-render-DATA-HORA-IDUNICO\`, com `session.json`,
`capture.tsv`, imagens `native-NNNNNNNN.ppm` e o marcador quando manual. Imagens
PPM P6 podem ser abertas em visualizador compatível; são RGB nativo, sem escala.
O frame marcado M inclui M-8..M+16; início precoce gera `pre_missing`. Aguarde
até M+16 e a mensagem de exportação antes de encerrar o jogo. Uma captura por
processo; para outra, encerre e execute o script novamente. Nenhum arquivo de
capturas anteriores é substituído. Guarde a pasta inteira, o EXE correspondente
e `notes.txt` com cena, inputs, defeito presente/ausente e configurações legíveis.
Não acrescente ROM ou dumps de memória a essa pasta.

O ring verifica teto de 64 MiB, suporta largura até 800 e 224 linhas, limita
eventos PPU a 8192/frame e DMA a 512/frame. Registra perdas separadas. Os hashes
de memória são FNV-1a: regiões 0..15 = blocos de 4096 bytes VRAM, 16 = CGRAM,
17 = OAM baixo; amostrados em cpu_begin e draw_end, sem payload. Não representam
o conteúdo consumido a cada pixel. Eventos de ports de conteúdo são ocultados
antes de entrar no ring. GSU retém amostras de 64 jobs recentes, com perdas não
quantificáveis pela API atual. A apresentação GPU não é capturada neste modo.
Estado/registros PPU por linha estão em `regs64`; offsets com nomes estão no
cabeçalho do TSV. `hdma_sample` é estado por linha, não timestamp de barramento.

Há I/O para verificar o marcador uma vez por frame. A exportação usa uma thread
SDL separada e apenas um lote, sem espera pelo disco no loop principal. Aguarde
a mensagem final antes de sair: encerrar o processo pode interromper arquivos.
Falha de criação da thread perde o lote e é informada em stderr. O custo real
de CPU/I/O concorrente ainda depende de medição local. O script desabilita o trace
legado no filho para evitar seu I/O contínuo. Reset aborta a coleta; save/load,
rewind, netplay, turbo, frameskip e run-ahead invalidam a comparação proposta.
Veja `YoshisIsland_errorRender.md` para limitações e roteiro individual.

## Trace legado por intervalo

Instrumentação experimental; não corrige o renderer nem confirma uma causa.
Esta build experimental substitui o loop personalizado por `beam_frame_driver`
local: fronteira V=225, IRQs na simulação e replay com HDMA real. Não há IRQ
guest executada pelo loop de desenho. A resolução das piscadas exige teste.
Compile localmente e copie o executável atualizado para `G:\x2` pelo seu fluxo
habitual. Execute em uma sessão offline, sem rewind/save-load/run-ahead; o contador
é de chamadas do port desde a abertura do processo, não um contador serializado.

Abra PowerShell e execute (a pasta nova preserva as coletas anteriores):

```powershell
Set-Location G:\x2
$captureDir = Join-Path (Get-Location) ('yoshi-trace-' + (Get-Date -Format yyyyMMdd-HHmmss))
New-Item -ItemType Directory -Path $captureDir | Out-Null
$env:YI_TRACE_FILE = Join-Path $captureDir 'trace.tsv'
$env:YI_TRACE_START = '640'
$env:YI_TRACE_FRAMES = '60'
$env:YI_TRACE_SCENE = 'intro-sem-input'
$env:SNESRECOMP_PRESENT_LOG = Join-Path $captureDir 'presents.csv'
$env:SNESRECOMP_SCREENSHOT_FROM = '640'
$env:SNESRECOMP_SCREENSHOT_TO = '699'
& ".\Yoshi's Island recomp.exe"
Remove-Item Env:YI_TRACE_FILE, Env:YI_TRACE_START, Env:YI_TRACE_FRAMES, Env:YI_TRACE_SCENE, Env:SNESRECOMP_PRESENT_LOG, Env:SNESRECOMP_SCREENSHOT_FROM, Env:SNESRECOMP_SCREENSHOT_TO
```

Confirme o nome do executável copiado. Use uma execução nova para cada cena:
intro, tela inicial e ilha antes do save. Ajuste START para incluir alguns frames
visíveis antes/depois da piscada e registre os inputs usados para chegar à cena.
Altere também SCREENSHOT_FROM/TO para o mesmo intervalo ao mudar START/FRAMES;
essas variáveis delimitam o CSV de apresentação existente no framework.
SCENE é uma etiqueta manual, não detecção de estado do jogo. START padrão=640,
FRAMES padrão=60, mínimo=1, máximo=120. FILE é obrigatório e o arquivo deve ser
novo (abertura exclusiva, erro no stderr se já existir ou o caminho for inválido).
O port não cria diretórios nem salva arquivos quando FILE está ausente.

## Formato TSV v2

O cabeçalho identifica `driver=beam_frame_driver`. Na v2, `drawn` substitui
`applied`: o observador é chamado imediatamente depois de ppu_runLine e antes
do HDMA do HBlank. O estado é o da linha recém-desenhada. Escritas entre callbacks
recebem a próxima linha de desenho; inicialização usa linha 0 e restauração
final pode usar 225. A captura antiga v1 permanece válida para comparação.

Prefixo comum: tipo, frame do port (primeiro=1), fase (`cpu`/`draw`), linha de
desenho (-1 antes do desenho), master_cycles CPU, beamMasterLast, V, H,
clock do processo em ticks. O cabeçalho inclui epoch e CLOCKS_PER_SEC; `clock`
mede tempo de CPU, não tempo de parede de alta resolução. Master clocks são a
referência principal. V/H podem estar atrasados em relação ao clock da CPU;
não são timestamps de barramento por byte, especialmente no DMA.

`write`/`boundary225`: endereço hex, valor hex (ou `redacted`), origem
(0 CPU, 1 DMA, 2 HDMA, 3 replay), forced blank (somente nos valores não ocultos;
significativo em 2100). Registros 2100–2133 são observados na entrada de
ppu_write; os valores dos ports de conteúdo 2104/2118/2119/2122 são ocultos.
420C não passa por este observador. Na v1, a fase `draw` também podia executar
código guest; na v2 o driver não executa handlers guest durante desenho.
Origem=3 identifica replay explicitamente.

`begin`, `cpu_end`, `drawn`, `row`, `draw_end`, `boundary_state`: INIDISP hex,
forced blank, brilho, BGMODE hex, TM hex, TS hex, count do journal selecionado,
dropped desse journal, cursor de replay, have_completed, eventos omitidos pela
captura neste frame, janela de 64 bytes de registradores PPU em hex (layout
Ppu.inidisp..cgwsel da revisão local, não endereços 2100 sequenciais),
GSU master_clock, instruction_count, clock_credit, SFR hex, PBR hex, R15 hex,
irq_pending, SCMR hex, SCBR hex, hash FNV-1a RGB da linha, pixels RGB não pretos.
Hash/pixels são NA exceto em `row`; não incluem padding nem byte X.
Linha de desenho L=1..224 corresponde à linha de framebuffer L-1.
`drawn` ocorre depois de ppu_runLine e antes do HDMA do próximo HBlank.
`boundary_state` amostra o journal de gravação antes de trocar de campo.
Durante desenho os contadores referem-se ao campo selecionado para replay;
fora dele, ao campo em gravação. O cursor fora do desenho pode ser antigo.

Há no máximo 16.384 eventos write/boundary por frame; os excedentes incrementam
o contador de omitidos, independente de `dropped` do journal (capacidade 4096).
Linhas de estado não são descartadas. Ao terminar o intervalo o arquivo é fechado
e o observador removido. A configuração é lida uma vez por processo.

## Comparação

Compare `drawn` e `row` de um frame visível e um preto, sobretudo INIDISP,
forced blank, TM/TS, fronteiras V=225, dropped e andamento do GSU entre begin e
cpu_end. Todas as linhas com pixels=0 demonstram saída nativa preta antes da
apresentação; hashes diferentes por si só não medem luminância nem provam erro.
Cruze os contadores com presents.csv; confirme o alinhamento por uma captura
visual, pois rewind, renderizações auxiliares ou frames omitidos podem separar
os contadores. Saída nativa visível com tela preta exige investigar a composição
e apresentação posteriores: este trace não observa o framebuffer final da GPU.

Forneça trace.tsv, presents.csv, configurações, revisão/binário utilizado,
sequência de inputs e frames/horário em que observou cada piscada. Não envie ROM
ou dumps de memória. Não há nova captura ou validação de runtime implícita nesta
instrumentação.

## ER-06: sondas de TM/TS do raster journal

A build de investiga��o exige tamb�m `yi-render-raster-probe-v1`. Use o mesmo
comando `scripts/capture-render.ps1 -ErrorId ER-06`; a janela continua sendo
5 segundos ap�s Enter, sem hist�rico anterior e com pasta �nica por execu��o.
Primeiro capture o seletor est�vel, se dispon�vel, com `-Scene 'seletor antes de
concluir fase; sem piscadas'`. Depois capture a ocorr�ncia com `-Scene 'seletor
apos concluir fase; com piscadas; anotar fase'`. Preencha `notes.txt` com a fase,
inputs e foco. Aguarde o marcador final antes da pr�xima coleta.

`raster_probe` no TSV exporta apenas metadados: stage, clocks CPU/beam, V/H,
slot em grava��o, flags armed/have_completed/rendering, cursor e contagem/perdas
de cada slot. `live`, `slot0` e `slot1` t�m cinco bytes de registradores na ordem
INIDISP, TM, TS, CGADSUB, CGWSEL. O slot conclu�do � current^1 somente quando
have_completed=1; cursor s� deve ser interpretado no contexto do est�gio.
Est�gios: 1/2 antes/depois da fronteira, 3/4 antes/depois de renderBegin,
5/6 antes/depois de renderEnd, 7 in�cio CPU e 8 fim CPU. Estado por linha e
escritas continuam nos registros existentes. N�o h� snapshot de mem�ria guest.

Limite: 12 sondas/frame, sem aloca��o nem I/O por sonda; excedentes constam em
`raster_probe_summary lost`. O teto total de 512 MiB continua verificado em
runtime. Os hooks leem o estado, sem mudar as m�scaras nem a ordem do replay.
A nova API do framework e o port precisam ser compilados juntos. A build n�o
valida a hip�tese; a compara��o das pr�ximas capturas determinar� onde TM/TS
mudam ou s�o herdados.
