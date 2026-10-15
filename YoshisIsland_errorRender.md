# Diagnóstico ER-01 a ER-06 — estado em 15/10/2026

**Timing update:** the latest source protocol uses five seconds of preparation
followed by two seconds of capture. Older `timed5` details below record prior
versions; the updated EXE must be built locally before using the new script.

O registro original e as sete imagens foram preservados em `_errorRender.md` e
`_errorRender/`. Este documento atualiza o estado técnico, sem substituir as
observações originais. **ER-06 resolvido no cenário testado; ER-01 a ER-05 abertos.** A preparação
foi seguida por build e cópia autorizadas pelo usuário, registradas ao final.
Não houve execução do jogo, nova captura ou validação visual pelo agente.

## Fatos e inspeção

- O executável atual `G:\x2\SuperMarioWorld2YoshiSIslandSNESRecomp.exe` foi lido:
  SHA-256 `3320A1C1391DE0A67F711C1A0B48AEA84161E0D9E81177660309CD667B99D095`.
  Ele é a referência anterior, não contém automaticamente esta instrumentação.
- A raiz usada por padrão no CMake é `modules/snesrecomp`. Foram consultados
  FRAME_MODEL_HOSTS, beam_frame_driver, PPU, DMA, cartucho, Super FX e os logs
  existentes. Confirme seu SNESRECOMP_ROOT no build local.
- HEAD do port lido: `048b5f5a6a9d6e51514e31623c8fb9e4bd61e30e`.
  A revisão Git interna do framework não pôde ser autenticada: acesso negado ao
  gitdir do submódulo. O script registra hashes dos fontes locais; isso não prova
  que um executável foi compilado deles.
- Após correção do caminho pelo usuário, foi lido
  `F:\GitRevised\SnesRecompProject\docs\specialChips\superFX2.md`. A nota confirma
  a validação anterior restrita à intro, os limites de autenticação do pin e a
  necessidade de qualificar outras cenas. Não foi alterada nesta etapa; suas
  referências ao plano ainda não implementado descrevem o estado histórico.
- A imagem 5 foi inspecionada: há descontinuidade no topo e uma linha no horizonte;
  isso não determina linha SNES exata, origem, intermitência ou causa.

## Implementação e limites (build posterior registrada ao final; runtime pendente)

Arquivos alterados/criados: `src/game_rtl.c`, `src/render_capture.h`,
`tools/render-capture.ps1`, `tools/flicker-trace.md` e este documento.
Na continuação foram alterados `src/render_capture.h`, `tools/flicker-trace.md`
e este documento, e criado `scripts/capture-render.ps1` para jogo já aberto.
Nenhum arquivo do framework foi alterado. O beam_frame_driver foi preservado.

O modo legado é opt-in (`YI_CAPTURE_DIR`), uma captura por processo, 8 frames anteriores,
frame marcado e 16 posteriores. O ring pré-aloca 25 slots com teto verificado de
64 MiB. Cada slot contém até 8192 eventos, 224 linhas com estado PPU aplicado,
amostras HDMA/GSU e imagem RGB até 800×224. A quantidade exata alocada é informada
em stderr e no TSV por `sizeof`, sem pressupor tamanho ABI de estruturas.
Não há alocação nem escrita em disco por evento/linha. A cada início de frame
há uma consulta ao arquivo sinalizador; no final de cada frame são copiadas até
512 entradas DMA e os 64 jobs GSU mais recentes. O custo real de CPU/latência
ainda precisa ser medido localmente; não há benchmark nesta etapa.

O modo atual de `scripts/capture-render.ps1` arma com o jogo já aberto e captura
**somente os 5 segundos de relógio após Enter**, sem histórico anterior.
O script é portátil, identifica o processo (ou recebe `-GameProcessId`), publica
`yi-capture-request-PID.txt` com segunda linha `timed5` ao lado do EXE e espera
Enter no console. O port consulta o pedido a cada 30 chamadas enquanto desarmado,
pré-aloca até 360 slots com teto de **512 MiB**, e informa `capture.armed` com
marcador `yi-render-timed-v1`. O custo real é registrado por `sizeof`; não há
alocação por evento/linha. A memória do lote temporizado permanece alocada no
processo para proteger um produtor interrompido em um slot; após a exportação e
a liberação do slot atual, a mesma alocação pode ser reutilizada.

Ao pressionar Enter, o script publica um prazo UTC de 5 s no `trigger`, mostra
contagem regressiva monotônica e encerra ao fim da janela após salvar o manifest.
Não há espera por exportação no script, nem `-WaitForExport`/`-TimeoutSeconds`.
O receptor calcula o tempo restante ao ler o pedido, sem prolongar uma ativação
tardia, e usa contador monotônico depois disso. Uma thread independente interrompe
a coleta no prazo mesmo se o guest parar de avançar e exporta somente slots
completos publicados por atomics. O frame ainda em processamento é excluído.
Se o jogo só ler o pedido depois do prazo, exporta zero frames como falha.
O script não encerra o jogo, não pausa e não envia Enter ao guest.

A exportação pode terminar após o script encerrar; `capture.done`/`capture.failed`
continuam sendo a confirmação final. Fechar o jogo antes disso pode interromper
a gravação. O manifest do script registra apenas arquivos/status observados aos
5 s. Um atraso de filesystem ou escalonamento pode atrasar o encerramento real
do script: a janela solicitada é 5 s, não uma garantia de latência de disco.
A 60 Hz, com o jogo avançando durante todo o período, são esperados cerca de
300 frames; o limite é 360. Frames além do limite não substituem os anteriores:
`frame_cap_drops` registra a perda. Isso não garante capturar perfeitamente um
evento, em especial se o host pausar ao perder foco. Reset exporta o lote parcial.
A próxima captura pode ser armada no mesmo processo depois que a exportação
terminar. Não há correção de renderização aqui.

O script verifica os marcadores `yi-render-timed-v1` e
`yi-render-repeat-v1` no EXE antes de criar outra pasta. Assim, uma build antiga
que aceita só a primeira coleta não é confundida com o receptor rearmável.
É necessário seu build local atualizado; o executável de 25 frames não é aceito
como receptor temporizado. Não substituir o arquivo de um processo em execução.
A pasta do EXE precisa ser gravável. O script usa UTF-8 com BOM para PowerShell
5.1 e pedido UTF-8 sem BOM; o receptor Windows converte caminhos para `_wfopen`.
Pode ser copiado para `scripts/` ao lado do EXE, sem depender do checkout. As
coletas ficam por padrão ao lado do EXE (`G:\x2` no fluxo do usuário), sem
artefatos na pasta de build. Cada execução cria pasta única com data/hora e ID;
pedidos pendentes impedem duplicação, e arquivos antigos são preservados. Hashes
de fontes são opcionais quando o checkout está disponível.
O modo legado por `getenv` ainda não ganhou conversão própria para caminhos
acentuados; para esses caminhos use o script de attach.

`capture.tsv` v2 inclui prazo UTC, frequência do contador host, `host_interval`
por frame, quantidade retida, perdas por limite, eventos/linhas/metadados/hashes
já descritos. O exportador percorre slots publicados, mesmo se os IDs do port
não forem contíguos. Zero frames é falha, não sucesso. Os limites de 8192 eventos
e 512 DMA por frame continuam, com perdas separadas.

No modo legado abaixo, `trigger` marca o próximo frame que o consultar. `YI_CAPTURE_AT` marca por contador
de chamadas do port desde o processo (primeiro=1). O contador não é serializado.
Reset aborta o lote atual; após a exportação, outro pedido pode armar nova coleta
no mesmo processo. Não usar save/load, rewind, turbo,
frameskip, run-ahead, netplay ou ressimulação para comparar os contadores.

A exportação após o último frame transfere o ring para uma thread SDL separada,
sem esperar pelo disco no loop do jogo. Não há fila crescente: é um único lote.
Falha ao criar a thread perde o lote e é informada em stderr; não há fallback
síncrono. Ainda existe custo de CPU/I/O concorrente e consulta ao marcador no
loop principal. O processo deve continuar até a mensagem final; sair antes pode
interromper arquivos, pois a thread é destacada. O legado `YI_TRACE_FILE`, se habilitado manualmente junto, continua
fazendo I/O durante a janela; o script o desabilita apenas no processo filho.

## Dados e limites

- `capture.tsv`: IDs do port, CPU/beam master clocks, V/H, fase, linha de desenho,
  origem das escritas; valores de ports de conteúdo substituídos por 256 antes de
  reter o evento. Fronteiras V225 são eventos, não um contador PPU independente.
- `row`: y=0..223, validade, largura/pitch original, hash FNV-1a BGR sem padding/X,
  pixels não pretos, journal/drops/cursor/completed, GSU clock/instructions/SFR/IRQ,
  janela de 64 bytes de registradores PPU. O cabeçalho nomeia offsets locais;
  inclui scroll, Mode7, janelas, layers, máscaras e color math. Não são payloads
  VRAM/OAM/CGRAM. É estado após desenho, antes do HDMA seguinte; limites de janela
  calculados internamente pelo compositor não são observados.
- `native-NNNNNNNN.ppm`: RGB P6, linhas copiadas no observador, largura constante,
  sem redimensionar. Linhas inválidas impedem exportar uma imagem válida; aparece
  `image_invalid` e pode ficar arquivo vazio para sinalizar falha. Nenhuma linha
  inválida recebe pixels inventados. Não se exportam dumps de memória guest.
- `hdma_sample`: frame/y/canal/ativo/terminado/tableAdr/repCount/doTransfer/indirect/
  bAdr/mode/size (size é endereço indireto quando aplicável). Amostra, não evento
  exato de início/fim. As escritas PPU resultantes têm origem própria nos eventos.
- `dma`: uploads retidos entre começo/fim da chamada, com contador próprio do
  framework explicitamente separado, canal, origem/destino, direção e tamanho.
  Sem conteúdo e sem precisão temporal por byte. Limite 512/frame, `dma_lost`.
- `job_recent`: até 64 jobs mais recentes, início/fim master clocks e PC inicial;
  stop=0 significa job ainda aberto. Entradas repetem entre frames. A API não
  fornece contagem total/expulsões: `gsu_evictions=unknown`; 64 entradas não provam
  cobertura completa. Progresso também é amostrado por linha. Durante replay os
  clocks GSU/beam podem ficar constantes, o que não indica travamento.
- `event_lost`, journal `dropped`, `invalid_rows`, `missing`, `pre_missing` e
  falhas de I/O são distintos. Não há contador de perdas de apresentação nem
  associação a um framebuffer final GPU. `memory_hash` amostra em cpu_begin e
  draw_end os 16 blocos de 4096 bytes VRAM (regiões 0..15), CGRAM (16) e OAM
  baixo (17), apenas FNV-1a, sem payload. OAM alto não está incluído. Esses hashes
  não observam a memória efetivamente consumida por linha; se ER-02 exigir essa
  precisão, será necessária outra extensão delimitada. Hashes por linha localizam diferenças
  visuais; não identificam tiles responsáveis.
- `session.json`: identidade/configuração do executável, hashes dos fontes e
  config/keybinds presentes, cena/ID. Registrar configurações legíveis e inputs
  em `notes.txt`; hashes de config não explicam seu conteúdo. Fontes e binário
  devem ser associados pelo usuário após seu build local.

## Plano individual e critérios para o próximo teste

1. **ER-03 primeiro:** título, frames com/sem defeito; examinar y=0 e vizinhas,
   pitch/largura/validade, estado aplicado e início do HDMA. A captura nativa pode
   localizar a divergência antes da apresentação. Captura equivalente da janela
   ainda deve ser identificada manualmente; a imagem GPU não está implementada.
2. **ER-01:** cena das nuvens com faixa presente/ausente; localizar y pela imagem
   nativa, comparar linhas vizinhas, estado de janela/layers/math e eventos.
   Não inferir causa por coincidência com HDMA ou GSU.
3. **ER-04:** título/horizonte separadamente; confirmar primeiro a diferença com
   referência equivalente, depois comparar y/estado aplicado. Não remover uma
   linha que ainda pode pertencer à imagem esperada.
4. **ER-02:** programar janela antes da transformação até depois da fragmentação;
   cruzar uploads/canal/destino/tamanho e jobs/progresso GSU. Histórico de 8 frames
   pode ser insuficiente para reação humana. Consistência de conteúdo exige
   futura instrumentação com hashes regionais, sem dumps, se necessária.
5. **ER-05:** programar antes da primeira caixa, incluindo aparição completa,
   desaparecimento e expansão. Comparar windows/máscaras/layers por linha,
   uploads e imagens sucessivas. Repetir caixas do gameplay e pause como controles;
   a observação do usuário nesses controles não valida todas as situações.
6. **ER-06:** capturar a seleção de fases já piscando após concluir uma fase;
   comparar com outra execução na seleção antes da primeira fase e, depois,
   gameplay sem piscadas relatadas. Registrar fase concluída, inputs, tempo de
   permanência no mapa, foco e configuração. Para localizar o início da falha,
   fazer outra janela programada cobrindo a transição de conclusão para o mapa;
   uma captura posterior à transição não recupera eventos anteriores ao Enter.

Para cada ID: registrar cenário/inputs, confirmar imagem e telemetria alinhadas,
perdas/limites, overhead e ausência de mudança nos controles. Repetir intro como
controle das piscadas de tela inteira já eliminadas no teste anterior. Nenhuma
causa ou correção destes seis IDs foi demonstrada.

## ER-06 — piscadas na seleção de fases após concluir uma fase

**Estado: aberto, relato do usuário em 15/10/2026; sem nova captura instrumentada.**

O usuário relata que a seleção de fases passa a piscar muito quando conclui uma
fase e retorna a ela. Ao continuar jogando, não percebe piscadas no gameplay;
ao retornar novamente à seleção, as piscadas reaparecem. Ainda não foi medido
se são quadros totalmente pretos, alternância localizada ou problema posterior
ao framebuffer nativo, nem se afeta todas as fases/saves/configurações.

É um caso separado das faixas ER-01/ER-04 e da intro sem piscadas no teste anterior.
A validação anterior da intro permanece restrita àquele cenário. O relato atual
não comprova que todos os trechos de gameplay estejam validados.

**Hipóteses a distinguir, não causas confirmadas:** estado de display/forced blank,
layers/janelas, replay/HDMA ou conteúdo disponível após a transição; associação
entre simulação e apresentação. Não ajustar clocks, limpar buffers nem ocultar
frames para tratar o sintoma sem evidência.

**Dados necessários:** sequência de frames nativos afetados/normais no mapa,
INIDISP e estado aplicado por linha, fronteiras V225, journal/drops, DMA/HDMA,
progresso/jobs GSU, hashes regionais e identidade/configuração do EXE. Comparar
imagem nativa preta/visível com observação da janela; a apresentação GPU ainda
não é capturada por esta instrumentação. Uma gravação externa equivalente deve
ter origem e estágio identificados antes de comparação com Bnes.

**Critério futuro:** concluir a fase, permanecer na seleção, entrar em outra fase
e retornar novamente sem piscadas adicionais; repetir o ciclo e conferir intro
e gameplay como controles. A correção continua pendente de diagnóstico e runtime.

### Tentativa do usuário — 15/10/2026, 08:45

Pasta preservada: `G:\x2\yoshi-render-20261015-084515-f5091229`.
Foram encontrados apenas `attach-request.txt`, `notes.txt`, `session.json` e
`trigger`. Não há `capture.armed`, `capture.tsv`, imagens, `capture.done` ou
`capture.failed`. O manifest registra `armedBeforeEnter=false` e
`status=trigger_requested`; o usuário interrompeu o script com Ctrl+C após Enter.
Esses arquivos documentam uma solicitação, **não uma captura de frames**.

O EXE em disco `G:\x2\Yoshi's Island recomp.exe` teve SHA-256
`5F293BB968A04B4378EFB20502DD9D12AB4CD1E6BA6A0970B5B0AA82FDBE67C8`.
Uma busca local por marcadores encontrou ausência de `yi-capture-request-`,
`yi-render-attach-v1`, `capture.armed` e `yi-render-v1`. Portanto esse arquivo não
foi identificado como contendo o receptor. Não foi inspecionada a imagem carregada
do processo; não atribuir o problema de renderização a esse resultado. Nenhum
frame novo está disponível para diagnosticar ER-06 nesta tentativa.

O script anterior estava em UTF-8 sem BOM, e tanto o console como os metadados
apresentaram acentos corrompidos. Correções nesta continuação: BOM no `.ps1`,
protocolo de caminhos UTF-8/Unicode, verificação do receptor antes de criar uma
pasta, encerramento após sinalizar por padrão e espera opcional explícita.
O script antigo e a pasta da tentativa foram preservados. Não houve build,
execução do jogo ou nova captura pelo agente; verificações foram estáticas.

A cópia em uso `G:\x2\scripts\capture-render.ps1` foi atualizada e seu SHA-256
conferido contra o script do repositório. Backup preservado:
`G:\x2\scripts\capture-render.before-20261015-085327-0283a15f.ps1`.
O parser do Windows PowerShell 5.1.26100.9444 aceitou a sintaxe, e o BOM UTF-8
foi confirmado. Isso não valida a captura em runtime; o suporte a caminhos
acentuados no receptor C continua dependendo de seu build e teste locais.

### Atualização posterior — janela fixa de 5 segundos

A pedido do usuário, o modo atual substitui o encerramento imediato após Enter
por uma contagem de **5 segundos**, sem espera adicional pelo jogo. O receptor
temporizado foi implementado nos fontes com limite de 360 frames/512 MiB,
publicação de slots completos por atomics e exportação independente do progresso
guest. O modo legado de 25 frames permanece separado. O script recusa o receptor
antigo para não apresentar 16 frames posteriores como coleta de 5 segundos.

Arquivos desta atualização: `src/render_capture.h`, `src/game_rtl.c`,
`scripts/capture-render.ps1`, `tools/flicker-trace.md` e este documento.
A cópia `G:\x2\scripts\capture-render.ps1` foi atualizada, com hash idêntico à
origem. Backup: `G:\x2\scripts\capture-render.before-5seconds-20261015-090213-2622b1e6.ps1`.

Verificações: parser do Windows PowerShell 5.1 sem erros, BOM confirmado,
expressão de prazo Unix em ms aceita no mesmo ambiente, inspeção dos limites,
protocolo, publicação atômica e diff. Nenhum build, execução do jogo ou nova
captura pelo agente. Memória/overhead real, precisão observada da janela e
integridade dos arquivos continuam dependendo do teste local do usuário.

### Build instrumentada entregue — 15/10/2026

O usuário autorizou explicitamente nova build e cópia para a pasta de teste,
substituindo a proibição padrão de build apenas para esta solicitação.

- Configuração existente: `build-ninja`, Release, GCC/MinGW, SDL3 estático,
  framework efetivo em `modules/snesrecomp`.
- O primeiro build passou, mas mostrou histórico PPU/DMA desativado para produção.
  Foi habilitado `SNESRECOMP_ENABLE_PPU_DMA_HISTORY=ON` no cache local e o alvo
  foi recompilado com sucesso. Nenhum fonte precisou de correção nesta build.
- Comandos: `cmake -S . -B build-ninja -DSNESRECOMP_ENABLE_PPU_DMA_HISTORY=ON`
  e `cmake --build build-ninja --config Release --target SuperMarioWorld2YoshiSIslandSNESRecomp --parallel 4`.
- Origem: `build-ninja/Yoshi's Island recomp.exe`, 13.154.351 bytes.
- Destino: `G:\x2\Yoshi's Island recomp.exe`.
- SHA-256 da origem e destino, iguais:
  `78356A2F5808D300AB2A2C51EE642CEE5E63668DFF466E7AE26A69D23DDF380B`.
- Backup anterior preservado:
  `G:\x2\Yoshi's Island recomp.before-instrumentation-20261015-091256-3d4a6217.exe`.
- Marcadores encontrados no novo EXE: `yi-render-timed-v1`,
  `yi-capture-request-`, `capture.armed`, `capture.done` e `yi-render-v2`.
- O script em `G:\x2\scripts\capture-render.ps1` tem hash idêntico ao atual do
  repositório. Não foram copiados ROM ou dumps, nem criadas capturas no build.

A compilação e a identidade da cópia foram verificadas; o jogo não foi executado.
Fechar eventual processo antigo e abrir o EXE novo antes de testar. Para ER-06,
com o jogo novo aberto, executar em `G:\x2`:
`.\scripts\capture-render.ps1 -ErrorId ER-06`. Após Enter, o script conta 5 s e
encerra; aguardar `capture.done` ou `capture.failed` antes de fechar o jogo.
Os seis problemas permanecem abertos até evidência de runtime correspondente.

Bnes só será comparado com versão, configuração, fonte da captura e estágio da
animação identificados. Não há comparação equivalente disponível nesta sessão.
Preservar `G:\x2\yoshi-flicker-diagnostic`, `yoshi-trace-20261014-201927` e
`yoshi-trace-20261014-202555`; o novo script cria pastas únicas e não as modifica.

Comandos e preservação: veja `tools/flicker-trace.md`. Verificações nesta etapa:
inspeção estática dos contratos e diff; nenhuma compilação, execução do jogo ou
captura nova. A implementação permanece experimental e dependente do teste local.

### Descoberta do processo ajustada — 15/10/2026

O usuário tentou abrir `Yoshi's Island recomp.before-instrumentation-...exe`;
o filtro de nome exato do script retornou zero antes de inspecionar o binário.
A descoberta agora aceita nomes com o prefixo do jogo. A checagem posterior do
marcador `yi-render-timed-v1` ainda rejeita o backup antigo. Use `Yoshi's Island
recomp.exe`, a build instrumentada nova. Se houver mais de uma instância, feche
as extras ou indique `-GameProcessId`.

Atualizados `scripts/capture-render.ps1` e sua cópia
`G:\x2\scripts\capture-render.ps1`; o backup anterior foi preservado em
`G:\x2\scripts\capture-render.before-process-detection-20261015-091818-91ef0b66.ps1`.
Parser do PowerShell 5.1: sem erros. Nenhum jogo foi executado ou capturado.

### Nova tentativa do usuário e suporte a execuções repetidas — 15/10/2026

O usuário relata que uma execução após concluir uma fase não mostrou o defeito;
depois de concluir outra fase, a seleção de fases piscou novamente, mas não foi
possível iniciar o script outra vez no processo aberto. Nenhuma captura dessa
segunda ocorrência foi obtida; portanto não há frames adicionais para análise.

O script cria uma pasta por execução com data/hora e sufixo único, e agora avisa
para aguardar `capture.done` ou `capture.failed` da coleta anterior. Para aceitar
outro pedido no mesmo PID, o receptor consome o arquivo de rendezvous e rearma o
ring apenas depois que a thread anterior termina a exportação. A memória continua
limitada à alocação original, sem criar um segundo ring. Pastas existentes não
são removidas nem reutilizadas.

Arquivos alterados nesta atualização: `src/render_capture.h`,
`scripts/capture-render.ps1`, `tools/flicker-trace.md` e este documento. A cópia
em `G:\x2\scripts\capture-render.ps1` foi atualizada e comparada por SHA-256.
Backup preservado: `G:\x2\scripts\capture-render.before-repeat-20261015-092731-90a75f05.ps1`.
Após adicionar a checagem de `yi-render-repeat-v1`, a cópia recebeu outra
atualização. Backup: `G:\x2\scripts\capture-render.before-repeat-marker-20261015-092915-4b4a2b0a.ps1`.
SHA-256 atual do script de origem e da cópia: `8EC411C7233F8C7256543A3CC8F12A77EC75DD6F2BE40BBADFC190651A09D6CB`.
Verificação somente de leitura do EXE atual em `G:\x2`: SHA-256
`78356A2F5808D300AB2A2C51EE642CEE5E63668DFF466E7AE26A69D23DDF380B`; contém
`yi-render-timed-v1`, mas **não** `yi-render-repeat-v1`, então o script atualizado
vai recusá-lo antes de criar a próxima pasta.
### Build de rearme e cópia para teste — 15/10/2026

Build Release/Ninja concluída com a configuração existente
`SNESRECOMP_ENABLE_PPU_DMA_HISTORY=ON`, sem alterações de fontes durante esta
build. Comando: `cmake --build build-ninja --config Release --target
SuperMarioWorld2YoshiSIslandSNESRecomp --parallel 4`.

- Origem: `build-ninja/Yoshi's Island recomp.exe`, 13.154.390 bytes.
- Destino: `G:\x2\Yoshi's Island recomp.exe`.
- SHA-256 de ambos: `D1DF50F951C346A66D4B03D8340A2D74518AED081F762FB4700069B85E12CF54`.
- Marcadores conferidos no EXE: `yi-render-timed-v1`, `yi-render-repeat-v1` e
  `capture.rearmed`.
- Não havia EXE no destino para backup. Nenhuma outra pasta ou captura foi
  alterada; nenhum ROM ou dump foi copiado.
- O EXE anterior de `build-ninja` (SHA-256
  `78356A2F5808D300AB2A2C51EE642CEE5E63668DFF466E7AE26A69D23DDF380B`) foi
  substituído pelo link da build; não havia outra cópia desse EXE nas pastas
  `build-ninja` ou `G:\x2` quando foram conferidas.

O script e o EXE de teste agora reconhecem capturas repetidas no mesmo processo.
Abra `G:\x2\Yoshi's Island recomp.exe`, execute
`.\scripts\capture-render.ps1 -ErrorId ER-06`, espere `capture.done` ou
`capture.failed` antes da próxima execução, e informe qual pasta apresentou o
problema. Cada execução cria sua própria pasta identificada por data/hora e ID.

O parser PowerShell aceitou os scripts de origem e destino, os hashes do script
continuam iguais e `git diff --check` passou. Não executei o jogo nem fiz nova
captura; rearme em runtime, qualidade da coleta e estado de ER-06 continuam
dependendo do teste local.


### ER-06: captura analisada e instrumentação de baselines — 15/10/2026

Captura preservada: `G:\x2\yoshi-render-20261015-093437-81dd767d`.
São 300 frames nativos 256x224, IDs 6522–6821, sem perdas registradas de eventos,
DMA ou journal, sem linhas inválidas nem falhas de I/O. Há uma fronteira V225
por frame. `capture.done` confirma a exportação posterior ao status pendente
salvo pelo script. O EXE identificado no manifest tem SHA-256
`D1DF50F951C346A66D4B03D8340A2D74518AED081F762FB4700069B85E12CF54`.

Fatos: os primeiros 28 frames têm INIDISP=8F, depois há brilho 00..0F.
No trecho 6565–6821, o mapa completo (129 frames ímpares) alterna com o cenário
incompleto (128 frames pares, todos com imagem idêntica). Em 6601/6602, todas
as linhas diferem apenas nos bytes TM/TS entre os 64 registradores amostrados:
17/00 no completo e 04/01 no defeituoso. INIDISP=0F nos dois. Os hashes VRAM e
CGRAM amostrados coincidem nesse par; OAM difere. O GSU continua progredindo.
Não há escritas TM/TS observadas na janela que expliquem essa alternância.
A imagem defeituosa já está no framebuffer nativo; isso não exclui problemas
adicionais de apresentação. Não houve comparação com Bnes.

Hipótese a testar: as baselines dos dois campos do journal podem estar herdando
máscaras distintas, e o estado após renderEnd pode alimentar a próxima baseline.
A observação anterior não prova essa origem. Não corrigir ou fixar TM/TS antes
de localizar onde o estado diverge.

Preparação: API somente de leitura `ppu_rasterProbe` e observador de estágios
separado no framework, sem modificar o contrato de PpuDisplayObserver. Sondas
antes/depois de V225, renderBegin e renderEnd, além de cpu_begin/cpu_end.
Capturam clocks e metadados dos dois slots, mais INIDISP/TM/TS/CGADSUB/CGWSEL
live e nas baselines. Limite 12/frame, perdas explícitas em
`raster_probe_summary`, sem alocação/I/O por sonda. Imagens/eventos antigos
permanecem no formato existente. Nenhum payload de memória foi acrescentado.

Arquivos: `modules/snesrecomp/runner/src/snes/ppu.c` e `.h`,
`src/render_capture.h`, `src/game_rtl.c`, `scripts/capture-render.ps1`,
`tools/flicker-trace.md` e este documento. Diferentemente das etapas anteriores,
esta instrumentação altera a cópia local do framework; seu pin Git não foi
autenticado nesta etapa. O script exige `yi-render-raster-probe-v1` para evitar
uma coleta sem as sondas. O comando e o prazo de 5 segundos permanecem iguais.

Backups anteriores à build preservados em G:\x2:
`Yoshi's Island recomp.before-raster-probe-20261015-094531-04437aa0.exe` e
`scripts/capture-render.before-raster-probe-20261015-094531-04437aa0.ps1`.
O backup EXE tem o SHA-256 D1DF50...12CF54 acima e também preserva o EXE anterior
de build, cujos bytes eram iguais. A captura analisada não foi alterada.


Build e entrega desta instrumentação concluídas: Release/Ninja, histórico
PPU/DMA ON, alvo SuperMarioWorld2YoshiSIslandSNESRecomp. O build recompilou
ppu.c e game_rtl.c e linkou com sucesso; houve aviso de FORCEINLINE redefinido
entre headers Windows e types.h, sem erro de compilação.

- EXE em build-ninja e `G:\x2\Yoshi's Island recomp.exe`: 13.156.634 bytes,
  SHA-256 `7215E80BF39E0BAD7D2D6F1486568428293E67A968157230971AF4107FA3CBEC`.
- Script em scripts/ e `G:\x2\scripts\capture-render.ps1`: SHA-256
  `18A1C621EEA3686D352AE8D5230668BC072C335284AE95AC1E44B117F2EF7340`.
- Marcadores verificados: yi-render-raster-probe-v1, yi-render-timed-v1,
  yi-render-repeat-v1 e raster_probe_summary.
- Parser Windows PowerShell 5.1.26100.9444 e BOM UTF-8 verificados. Hashes de
  origem/destino conferidos para EXE e script; git diff --check passou no port.
  A inspeção direta dos hooks confirmou a ordem antes/depois e a ausência de
  escrita em estado guest na nova consulta. O teto de memória segue verificado
  pelo receptor e o tamanho exato será informado pelo sizeof no TSV da coleta.

Próximo teste: abrir o EXE de nome normal (os nomes maiores são backups), rodar
`.\scripts\capture-render.ps1 -ErrorId ER-06` em G:\x2 e capturar quando o
seletor piscar. Se possível, obter outra coleta no seletor sem piscadas com
`-Scene 'seletor sem piscadas'`. Preservar cada pasta inteira e informar qual
contém o defeito. A exportação pode terminar depois dos 5 segundos do script;
aguardar capture.done/capture.failed antes de outra coleta ou de fechar o jogo.

Nenhum jogo foi executado e nenhuma captura nova foi feita pelo agente. A causa
continua sendo hipótese: não foi aplicada correção às máscaras ou ao journal,
e o efeito das sondas/overhead ainda depende do teste de runtime do usuário.


### ER-06: mecanismo observado e correção candidata entregue — 15/10/2026

Captura analisada e preservada: `G:\x2\yoshi-render-20261015-094907-75410bfb`.
EXE identificado no manifest: SHA-256
`7215E80BF39E0BAD7D2D6F1486568428293E67A968157230971AF4107FA3CBEC`.
São 300 frames (6017–6316), 300 imagens válidas e oito sondas por frame sem
perdas de sondas. Não há perdas DMA/journal nem falhas de I/O; ocorreram
11.321 eventos descartados em 6120 (9.260) e 6121 (2.061). Por isso não se
considera completo o stream de escritas desses dois frames. As sondas de
fronteira cobrem todos os 300 frames; os eventos de fronteira desses dois frames
não sobreviveram ao limite compartilhado de eventos. O status pending do
manifest antecede a exportação, confirmada por capture.done.

No frame 6119, sem perdas, o guest escreve TS=00 e TM=17 na linha 224.
A baseline seguinte captura esses valores. RenderBegin restaura a baseline
anterior 04/01; o desenho termina sem consumir a cauda na linha 224, e
RenderEnd mantém 04/01. O próximo cpu_begin herda esse estado. Nos 150 frames
6167–6316, sem perdas de eventos nem escritas TM/TS, renderBegin alterna entre
as duas baselines, e renderEnd mantém o valor usado no desenho. Os frames
6315/6316 mostram respectivamente o cenário incompleto/completo. Essa cadeia
localiza uma contaminação de estado entre reconstrução visual e simulação;
a remoção do sintoma ainda requer validação da correção no jogo.

Correção em `modules/snesrecomp/runner/src/snes/ppu.c`: guardar os 64 bytes
INIDISP..CGWSEL antes de restaurar a baseline visual e devolvê-los ao guest em
RenderEnd. Preservar também scrollPrev, scrollPrev2, m7prev, mosaicStartLine e
os quatro campos do port OAM já protegidos anteriormente. O armazenamento
estático total desse snapshot é 72 bytes, sem alocação por frame nem dump.
A restauração é por cópia, sem escritas no barramento; substitui a reaplicação
parcial do journal seguinte. Não fixa TM/TS, não remove frames, não modifica
clocks, imagens já desenhadas, conteúdo de memória ou as entradas do journal.
A telemetria de sondas permanece ativa.

Também foram atualizados o contrato local em
`modules/snesrecomp/docs/FRAME_MODEL_HOSTS.md`, este documento e o teste
`modules/snesrecomp/tests/ppu/ppu_raster_journal_test.c`.
O novo caso sem ROM reproduz escritas na linha 224, restauração completa dos
registradores/latches e dois campos seguintes sem novas escritas TM/TS.
Falhou no código anterior em memcmp do estado guest e passou após a mudança.
Os casos anteriores do mesmo teste (splits, escritas após V225, OAM e reset)
também passaram. O teste foi compilado/executado isoladamente em pasta temporária,
sem iniciar o jogo. O compilador desse teste emitiu avisos nos trechos existentes
sobre signedness, parênteses, variável não usada e fallthrough; não houve erro.

Build Release/Ninja concluída: recompilação de ppu.c e link do alvo
SuperMarioWorld2YoshiSIslandSNESRecomp, mantendo PPU/DMA history ON.
O EXE anterior de build e teste foi preservado antes da build (bytes iguais):
`G:\x2\Yoshi's Island recomp.before-er06-fix-20261015-095645-2d88101a.exe`,
SHA-256 `7215E80BF39E0BAD7D2D6F1486568428293E67A968157230971AF4107FA3CBEC`.

Novo EXE em `build-ninja/Yoshi's Island recomp.exe` e
`G:\x2\Yoshi's Island recomp.exe`: 13.156.635 bytes, hashes iguais:
`E8C4ABA393A42E315F3A09CB9BE176525061328F273C86986F212D769BBF6EE8`.
Marcadores das sondas, captura de 5 segundos e rearme verificados. O script não
foi alterado: origem e cópia de teste mantêm SHA-256
`18A1C621EEA3686D352AE8D5230668BC072C335284AE95AC1E44B117F2EF7340`.
Não foram copiados ROM ou dumps nem alteradas capturas anteriores.

Teste local pendente: abrir o EXE de nome normal, concluir fases, retornar ao
seletor e repetir o ciclo que disparava as piscadas. Capturar com o mesmo comando
`.\scripts\capture-render.ps1 -ErrorId ER-06`, inclusive um resultado sem
piscadas, e informar a pasta. Esperado nas sondas: os registradores live de
stage=6 devem retornar aos valores de stage=3 do mesmo frame, sem forçar as
baselines a valores constantes. Conferir também intro, gameplay e pause como
controles da alteração geral de preservação do estado PPU. A observação visual
positiva isolada ainda não certifica todos os cenários.

Estado: ER-06 com correção candidata entregue, pendente de runtime; ER-01 a
ER-05 continuam abertos. Nenhuma execução do jogo ou captura nova pelo agente.

### ER-06 validado e ER-05 priorizado — 15/10/2026

A correção está versionada no commit `493f08b` (`fix stage select flicker`),
conferido antes desta edição com árvore limpa no port. `_errorRender.md` agora
registra sintoma, mecanismo, correção e validação do ER-06. A captura preservada
`G:\x2\yoshi-render-20261015-095903-0001eecf` identifica o EXE corrigido
`E8C4ABA393A42E315F3A09CB9BE176525061328F273C86986F212D769BBF6EE8`.
O usuário repetiu o gatilho sem piscadas. Nos 300 frames, os cinco registradores
das sondas retornam ao estado pré-desenho; nos 213 frames finais o seletor fica
estável, TM/TS=17/00, sem alternância ou quadros pretos. São 300 imagens válidas,
sem perdas de sondas, DMA ou journal nem falhas de I/O. Houve 11.319 eventos
descartados em dois frames da transição, separados do gatilho e trecho estável.
Esta validação é restrita ao fluxo testado, sem declarar ER-01 a ER-05 resolvidos.

O usuário priorizou ER-05 e relatou deslocamento horizontal indevido da caixa
de texto da abertura. O aparecimento prematuro antigo também permanece no
roteiro, mas precisa ser reavaliado após ER-06. Não atribuir os dois sintomas
automaticamente à mesma causa. Nenhuma referência equivalente de Bnes disponível.

Preparação atual nos fontes, sem build/cópia de EXE ou nova captura: `src/render_capture.h`
ganhou modo opt-in `yi-render-box-v1`, solicitado pela terceira linha do pedido:
`box x y largura altura`. O script ativa esse modo apenas em ER-05, com região
inicial 0,0,256,112 em pixels nativos e `-BoxRegion` para ajuste. A região deve
estar dentro de 800x224; cada linha registra a largura efetiva recortada à imagem.
Nenhum valor de coordenada foi inferido das imagens ampliadas como posição exata
da caixa. O default é uma faixa de observação, não uma detecção do objeto.

`box_row` guarda validade, x/largura efetivos, hash RGB FNV-1a, contagem e extremos
dos pixels não pretos, soma das coordenadas x (centroide = sum_x/nonblack),
posição/comprimento do maior trecho não preto de cor exata uniforme e seu hash
de cor, e posição/comprimento do maior trecho preto. Empates escolhem o trecho
mais à esquerda; comprimento zero significa ausência, não posição x=0 válida.
Os extremos sem pixels não pretos usam sentinela 800. `valid=0` indica linha
inválida ou região sem interseção. Scroll BG1..BG4, W1/W2, brilho e TM/TS têm
nomes legíveis; os demais estados de janela e color math continuam no regs64.
Isso não são limites calculados pelo compositor nem rastreamento automático.
Comparar várias linhas e confirmar o bloco nas imagens para evitar confundir
fundo, letras ou expansão com translação horizontal.

Para reduzir a pressão de milhares de eventos de conteúdo já redigido, somente
no modo box os ports 2104/2118/2119/2122 são agregados em `box_bulk_writes` por
frame, port, origem e fase. Origem 4 agrupa valores desconhecidos. Não preserva
timestamp individual de cada byte; não é descarte silencioso. Eventos de controle
e fronteiras mantêm a ordem/timestamps e o limite de 8192, com `event_lost`.
Os metadados DMA/HDMA e jobs/sondas GSU existentes continuam separados.

Limites: até 224 `RcBoxRow` por frame, quatro ports × cinco origens × duas fases
de contadores, 360 frames temporizados e teto de memória de 512 MiB preservado.
`sizeof(RcBoxRow)` e total do ring saem no cabeçalho. Não há alocação nem I/O
por linha/evento; a medida percorre apenas o RGB da região já copiado para o
frame. O custo real adicional de CPU ainda precisa ser medido no teste local.
As imagens continuam nativas; não há exportação de VRAM/OAM/CGRAM/WRAM/SRAM/ROM.

Arquivos alterados nesta etapa: `_errorRender.md`, este documento,
`src/render_capture.h`, `scripts/capture-render.ps1` e `tools/flicker-trace.md`.
Links das imagens no registro principal foram corrigidos para `_errorRender/`;
nenhuma imagem foi movida ou alterada. Os dois documentos técnicos com trechos
de codificação mista foram normalizados para UTF-8 para preservar os acentos.
Não foram alterados o framework nem a correção de ER-06. O script mantém UTF-8
com BOM para Windows PowerShell 5.1, pedidos em UTF-8 sem BOM e duração de 5 s.

Próximo teste depende de build com os fontes atuais. A cópia de teste ainda não
tem box-v1; o script exige esse marcador em ER-05 antes de criar qualquer pasta.
Não foram atualizados os arquivos de G:\x2 nesta etapa. Ver instruções atuais
em `tools/flicker-trace.md`; preservar resultados e identificar caixa/mensagem,
fase da abertura, movimento observado, inputs e mudanças de foco em notes.txt.

Verificações desta etapa: parser do Windows PowerShell 5.1.26100.9444 sem erros;
validação isolada de defaults ER-05/ER-06, região personalizada e seis regiões
inválidas (coordenadas negativas, limites, dimensão zero, quantidade e overflow).
Esse teste executou só o prefixo de parâmetros, sem descobrir processo, criar
coleta ou iniciar jogo. BOM do script e UTF-8 dos documentos conferidos; revisão
dos limites dos loops, índices, clipping e contadores; git diff --check passou.
O receptor também evita consultar trigger sem um pedido inicializado ao rearmar
o ring retido entre coletas. Sem compilação do C ou teste de runtime nesta etapa.

## ER-05 — ampliação para objetos do cenário e novas sondas de fase

O usuário relata que a bola empurrável também desliza com a câmera antes do
contato. A coleta `G:\x2\yoshi-render-20261015-105833-0c2f6d70` contém 299
frames (8691–8989), sem perdas de eventos, DMA, journal ou sondas, sem linhas
inválidas e sem falhas de exportação. O campo Scene conserva o default antigo
da abertura; as imagens mostram gameplay. Não reclassificar a cena pelo texto
automático do manifest.

Um trecho idêntico de 14×14 pixels da bola foi acompanhado em 80 frames.
Somar a posição ao scroll BG1 do próprio frame produz até três pixels de
variação horizontal; usar o scroll do frame seguinte produz a mesma posição
nos 80 casos, inclusive verticalmente. Um detalhe idêntico do chão confirma
deslizamento relativo de um pixel nos frames 8755–8757. A hipótese principal
é mistura de objetos e câmera de campos diferentes. Isso ainda não comprova
o estado físico do guest, ausência de contato ou a causa da caixa de texto.
Não existe sequência Bnes equivalente identificada para comparação.

Instrumentação ampliada em `src/render_capture.h`, sem modificar o framework:

- `motion_probe`: scroll BG1–BG4, hashes de OAM baixa/alta e CGRAM e contador
  OAM por fase, junto às sondas existentes. O serial conta notificações da
  fronteira desde a instalação do observer, não campos guest independentes.
- `obj_phase`/`obj_slot`: geometria e assinatura de até 128 slots em duas fases
  (antes da fronteira e depois do renderBegin). Não são bytes de OAM nem IDs
  de tiles; slots não representam identidade persistente de objetos.
- Coordenadas SNES anteriores aos ajustes widescreen/HUD e tamanho nominal
  quadrado do renderer. Não representam visibilidade, clipping ou física.
- O script exige `yi-render-motion-v1`; o default da região passa a 256×224.
  Scene deixa de presumir abertura e notes solicita objeto, câmera e contatos.
- Tetos mantidos: 360 frames, 512 MiB, 12 sondas/frame com perdas registradas,
  duas fases × 128 slots. Sem alocação/I/O por registro; hashes adicionais
  percorrem 1.056 bytes por sonda. Custo real precisa do teste local.

Arquivos desta etapa: `src/render_capture.h`, `scripts/capture-render.ps1`,
`_errorRender.md`, este documento e `tools/flicker-trace.md`. O último contém
os comandos e controles sugeridos. Esta extensão é passiva; não há tentativa
de corrigir ER-05 ou compensar movimento. `obj_summary` registra tentativas
repetidas além das duas fases; fases ausentes são inválidas explicitamente.

Build Release concluída com sucesso pelo alvo
`SuperMarioWorld2YoshiSIslandSNESRecomp`, em `build-ninja`, mantendo
`SNESRECOMP_ENABLE_PPU_DMA_HISTORY=ON`. EXE e script copiados para `G:\x2`;
hashes de origem/destino iguais e os cinco marcadores de captura presentes.

- EXE: `224D61FD4AEE5A4D0748E7B8EC34B23F936D9570279EC535E0EAB74A2DD2ABFE`.
- Script: `98A754207080EB9F8B7F1A3824D269DA76535D4B87D34A0921DE074A5827B815`.
- Backups: `G:\x2\Yoshi's Island recomp.before-er05-motion-20261015-111151.exe`
  e `G:\x2\scripts\capture-render.before-er05-motion-20261015-111151.ps1`.

Parser PowerShell passou e o BOM UTF-8 do script foi preservado. Nenhuma
captura anterior foi alterada. Sem novo runtime executado pelo agente; conteúdo
das novas sondas, custo e reprodução dos controles dependem do teste local.

## ER-05 — janela de captura reduzida

O script agora agenda o início cinco segundos depois do Enter e o fim dois
segundos após o início. Mantém apenas os frames completos dentro dessa janela:
aproximadamente 120 por teste a 60 Hz, cerca de 480 em quatro casos. O script
mostra a contagem regressiva de preparação e termina depois dos sete segundos
totais; a exportação pode continuar até `capture.done` ou `capture.failed`.

O protocolo grava timestamps separados de início e fim. O receptor converte
ambos uma vez para o relógio monotônico e só publica slots durante a captura;
os cinco segundos de preparação não entram no ring. O cabeçalho TSV v3 separa
o frame em que o pedido foi notado, primeiro e último frames completos e os
dois prazos. Zero frames completos continua sendo falha.

Arquivos alterados: `scripts/capture-render.ps1`, `src/render_capture.h`,
`tools/flicker-trace.md`, `_errorRender.md` e este documento. **Nenhuma build foi
feita nesta etapa**, de acordo com a instrução atual do repositório para builds
locais. O EXE e o script em `G:\x2` continuam com a versão anterior até o
build/cópia local. O novo script exige `yi-render-timed-window-v2` e recusará
um EXE antigo antes de criar a pasta, evitando coleta com duração incorreta.
