# Coleta delimitada das piscadas

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
& '.\SuperMarioWorld2YoshiSIslandSNESRecomp.exe'
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
