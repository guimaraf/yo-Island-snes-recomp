# ER-07 — corrupção externa do boss 1-4 durante o zoom

Esta coleta investiga o relato atual de ER-07. **Não use
`capture-render-er07.ps1`**: aquele script histórico trata da passagem/porta,
problema hoje identificado como ER-10. Nenhuma causa do boss foi estabelecida.

O relato distingue o crescimento inicial pelo feitiço do crescimento adicional
após acertos com ovos, até o boss parar de se mover. A linha preta vertical nas
laterais foi relatada durante os acertos e deve ser analisada separadamente da
linha horizontal já observada. "Sprite" descreve a aparência, sem atribuição
à camada OBJ.

O padrão agora é **20 segundos**, exigindo o novo receptor com o marcador
`yi-render-er07-boss-20s-v1`. Trocar apenas o script não amplia o EXE antigo.

## Experimento de fase de BG2 (opcional, ainda não é correção)

A build `ER07-bg2-phase.exe` preserva o comportamento anterior quando aberta
normalmente. O novo script exige essa build para confirmar o modo do teste.
O experimento é ativado explicitamente por uma coleta:

```powershell
cd G:\x2
.\scripts\capture-render-er07-boss.ps1 -Scenario Zoom -Renderer B -Experiment DelayBG2 -DurationSeconds 20
```

Arme antes do crescimento, pressione Enter e volte ao jogo nos cinco segundos
de preparação. O ajuste começa com o pedido armado, permitindo preparar a cena
já com a política experimental; ele termina quando a janela para. Não pressione
R nem alterne políticas no meio da captura. Se o crescimento começar tarde,
registre-o em outra pasta; a falta do início não é compensada por histórico.

`DelayBG2` acrescenta BG2 à máscara existente de VOFS diferido, apenas para as
linhas em que o driver identifica escrita HDMA nesse registrador. Não muda
BG2HOFS, inicialização de HDMA, janelas, color math, produção GSU, controles,
configuração salva ou a política já existente de BG3. O driver restaura o estado
guest após cada desenho como antes. Isso testa uma hipótese de fase, não assume
que o atraso seja uma regra universal do SNES.

Para o controle com o mesmo executável:

```powershell
.\scripts\capture-render-er07-boss.ps1 -Scenario Zoom -Renderer B -Experiment Baseline -DurationSeconds 20
```

`Baseline` é o padrão e mantém somente o ajuste anterior de BG3. Não é preciso
repetir o controle se as capturas B preservadas já forem suficientes; ele serve
para uma comparação mais próxima com a mesma build. Espere cada exportação
terminar antes da próxima coleta.

Depois do zoom, uma segunda coleta `-Scenario Acertos -Renderer B -Experiment
DelayBG2` registra a luta e as bordas verticais. Observe separadamente: faixa
horizontal, contorno/listras pretas, topo e base do boss, crescimento, acertos,
redução e saída. Melhora de um sintoma não resolve automaticamente o outro.

A pasta inclui `capture.experiment`, o modo solicitado no manifesto e
`phase_line` no TSV: máscara `04` = somente BG3; `06` = BG2 + BG3. A máscara é a
política passada ao driver, não prova de transferência HDMA em toda linha;
`video_state` contém o scroll efetivamente consumido. O script verifica contagem
e máscara, e a análise posterior confere as chaves e os registros de vídeo.
Todos os arquivos e executáveis anteriores devem permanecer preservados.

## Primeira coleta: renderer padrão

Abra `G:\x2\Yoshi's Island recomp.ER07-bg2-phase.exe`, sem apertar R.
Deixe apenas uma instância do jogo aberta. Prepare o boss 1-4 **antes do zoom
e antes da corrupção**, usando o percurso e o gatilho que você conhece.
O script não inicia o jogo, altera controles ou injeta entradas. Por padrão,
`-Renderer Atual` preserva o renderer. A seleção A/B explícita abaixo altera
apenas o bit do renderer durante o desenho e restaura as flags do chamador.

No PowerShell:

```powershell
cd G:\x2
.\scripts\capture-render-er07-boss.ps1
```

Depois que o receptor armar, pressione Enter no console. Há **cinco segundos
de preparação**, seguidos de **vinte segundos de gravação**. Volte ao jogo na
preparação; dentro da gravação, deixe cerca de 1–2 segundos antes do efeito,
reproduza o zoom normalmente e continue por alguns segundos depois. Se couber,
continue com acertos de ovos, sem esperar o defeito para iniciar.
Não espere a corrupção aparecer para iniciar: não existe histórico anterior.

Se o zoom começa automaticamente ao entrar na sala, prepare a entrada antes
de armar e use `-Scenario EntradaBoss`. Se a gravação já começar defeituosa,
repita a partir de um ponto anterior. Se ambos os eventos não couberem na janela,
aproxime o ponto inicial do gatilho e faça novas pastas; não remova a primeira.

Preencha `notes.txt`: sala e identidade visual do boss, gatilho exato, fase do
zoom, lado/região corrompida, instante aproximado, duração, botões usados,
dispositivo/mapeamento, pausas/foco e se o antes/durante/depois coube na janela.
Registre backend, escala, widescreen, mods, limites de sprites e velocidade.

## Coleta dos acertos com ovos

Com o boss já grande, prepare-se antes de acertá-lo e execute:

```powershell
.\scripts\capture-render-er07-boss.ps1 -Scenario Acertos
```

Após os cinco segundos de preparação, registre cerca de dois segundos antes
do primeiro acerto, arremesse ovos normalmente e continue depois da linha preta.
Anote o número/instantes aproximados dos acertos e o lado afetado, indicando se
a linha acompanha o contorno ou ocupa uma coluna fixa da tela.

Aguarde a exportação anterior terminar antes de repetir; cada coleta cria uma
pasta nova. Não é necessário reiniciar entre duas coletas de vinte segundos.
`-DurationSeconds 10` mantém a opção curta. Para passar de uma coleta curta a
uma de vinte segundos no mesmo processo, reinicie o jogo: a alocação curta não
comporta a maior e o receptor recusará o pedido. Comece diretamente com vinte.

## Comparação explícita sem depender da tecla R

As capturas anteriores cobrem os estados da luta, mas não registraram renderer
A. A nova build aceita seleção explícita, confirmada pelo processo e depois
verificada em cada linha do TSV. Para a próxima coleta, use:

```powershell
.\scripts\capture-render-er07-boss.ps1 -Scenario Acertos -Renderer A -DurationSeconds 10
```

Não pressione R. Reproduza um trecho em que a linha esteja visível, começando
antes do evento. Pode ser o crescimento inicial (`-Scenario Zoom`) ou acertos;
não é necessário repetir toda a luta. Mantenha largura nativa para A, e a mesma
configuração/percurso das coletas B. Se desejar outra coleta B explícita:

```powershell
.\scripts\capture-render-er07-boss.ps1 -Scenario Acertos -Renderer B -DurationSeconds 10
```

`-Renderer Atual` continua aceitando a troca manual durante uma coleta. Selecionar
A/B não corrige o jogo e não altera entradas, estado guest, configuração salva
ou o padrão do executável. A seleção dura enquanto o pedido está ativo; as flags
são restauradas após cada desenho. Considere `row.renderer` como a evidência do
modo realmente consumido, mesmo quando um modo foi solicitado explicitamente.

## Teste sintético de BG2

`modules/snesrecomp/tests/ppu/ppu_bg2_boundary_test.c` usa somente gráficos e
mapas construídos no teste. `scripts/test-er07-bg2.ps1` compila e executa esse
programa separado, sem lançar o jogo ou acessar ROM. O programa já compilado
fica em `G:\x2\tests\ER07-BG2\ppu_bg2_boundary_test.exe` e pode ser executado:

```powershell
& 'G:\x2\tests\ER07-BG2\ppu_bg2_boundary_test.exe'
```

São 110 casos por par A/B, com 256 pixels verificados por renderer/caso: valores
de scroll das fronteiras observadas, diferentes scrolls horizontais e linhas
vizinhas. A expectativa vem de linhas vermelhas/verdes explicitamente criadas
nas origens 511/255, com as demais linhas transparentes. Ambos os renderers
passaram. O teste demonstra como interpretam os valores selecionados; ele não
executa o driver temporal/HDMA, não reproduz os gráficos reais do boss e não
prova qual linha o hardware deveria consumir. O experimento opt-in de atraso de BG2 descrito acima não está validado como
correção; o teste sintético não executa essa política no driver do jogo.

## Comparação opcional

Se puder repetir o mesmo evento, faça uma segunda execução com a mesma
configuração e percurso. Pressione **R do teclado uma vez antes da cena**:

```powershell
.\scripts\capture-render-er07-boss.ps1 -Run UmaTrocaR
```

Para uma troca dentro de uma mesma captura, use `-Run Alternancia` e anote
quando pressionou R. Esse procedimento só compara a mesma fase visual se o
efeito durar tempo suficiente; duas etapas distintas do zoom não são um par
equivalente. Nenhuma alternância é executada automaticamente pelo script.

O botão R do controle SNES é outra entrada. `Padrao`, `UmaTrocaR` e
`Alternancia` descrevem o procedimento; A/B é determinado por `row.renderer`
em cada quadro/linha. Não presuma que o modo inicial seja sempre o mesmo.

## Evidências e limites

- Imagens `native-XXXXXXXX.ppm` e TSV compartilham IDs de quadros do port.
- `row` e `video_state`: renderer efetivo, modo, scroll, mapas, janelas,
  máscaras, color math e registros Mode 7 no bloco `regs64`.
- `consumed_vram`: hashes das páginas efetivamente consultadas e estado do
  journal; sem payloads. `bg3_sample` só é válido em Mode 1 sem mosaico relevante.
- `obj_line`: hashes de OAM baixo/alto e CGRAM consumidos, OBSEL, flags de
  desenho e hash/contagem/limites do buffer OBJ nos 256 pixels nativos. Esse
  buffer é anterior à composição com backgrounds, janelas e color math;
  sua ocupação não identifica o boss nem prova sua visibilidade final.
- `range_over`/`time_over` são flags acumuladas do modelo PPU, não contagens
  independentes por linha nem comprovação de que um limite causou o defeito.
- `motion_probe`, `obj_phase` e `obj_slot`: hashes e geometria limitada dos
  128 slots nas fases de boundary/render. Slots podem ser reutilizados e os
  tamanhos seguem o modelo quadrado atual; não identificam objetos persistentes.
- `dma`, `dma_event`, `hdma_sample` e `box_bulk_writes`: metadados de transferências,
  configuração e contagem de portas de dados por origem/fase. O nome histórico
  `box_bulk_writes` é reutilizado para contagens; nenhuma sonda de caixa é ativada.
- `gsu_video`: SCMR, SCBR, POR e CLSR; relógios/instruções/SFR/IRQ e jobs recentes
  ficam nos registros existentes. Durante o desenho o guest está parado;
  amostras por linha não significam execução do GSU por scanline. A retenção
  dos jobs é limitada e não garante ausência de jobs antigos descartados.

O limite padrão é de 1240 quadros e 1536 MiB, com caps e perdas no TSV.
A alocação atual de captura ocupa aproximadamente 1,31 GiB, além do jogo.
Em dez segundos, os limites anteriores de 620 quadros e 768 MiB permanecem.
As pastas de vinte segundos podem ocupar aproximadamente o dobro do espaço;
nenhuma pasta anterior é removida automaticamente.
`capture.done` confirma exportação, não sucesso visual ou integridade completa.
O script verifica contagens, quadros consecutivos, imagens ausentes e renderer;
correspondência exata de chaves, hashes e perdas internas exigem análise posterior.
O framebuffer GPU apresentado não é capturado (`presentation=unavailable`).

Aguarde `capture.done` ou `capture.failed` antes de fechar/repetir. **Preserve cada
pasta completa**, inclusive imagens, TSV, manifesto, notas e marcadores. Evite
turbo, frameskip, rewind, netplay e save/load durante a janela. Não substitua o
EXE aberto; não exporte ROM, tiles ou dumps de memória. O observador da tecla R
recebe apenas os eventos da janela do jogo, sem captura global do teclado.

Esta build mantém a correção ER-09 e as demais correções anteriores.
O experimento de BG2 não está validado como correção do boss: serve para localizar o primeiro quadro defeituoso e
comparar origem gráfica, avaliação OBJ e composição antes de propor uma mudança.
