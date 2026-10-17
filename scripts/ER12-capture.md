# ER-12 — barra próxima da lava no castelo 1-4

Coleta passiva: **5 segundos de preparação + 5 segundos de gravação**.
Usa o jogo já aberto; não inicia o executável, injeta entradas, muda controles,
seleciona renderer ou ativa experimento BG2. Nenhuma correção é aplicada.

## Preparação e execução

O receptor novo exige seu build local com estes fontes. Depois de compilar,
reabra o jogo; copiar o script não atualiza um processo já carregado. O script
confere o marcador `yi-render-er12-passive-v1` no arquivo e na resposta do processo.
Não substitua o EXE enquanto estiver aberto. O hash em disco e os hashes dos
fontes não autenticam, sozinhos, a correspondência com a imagem carregada.

Prepare a sala antes de a barra aproximar-se da lava. Na pasta do projeto:

```powershell
.\scripts\capture-render-er12.ps1
```

Se o script estiver junto do executável em `G:\x2\scripts`, execute esse mesmo
comando a partir de `G:\x2`. A saída padrão fica ao lado do EXE aberto.
Para escolher outro destino ou distinguir processos:

```powershell
.\scripts\capture-render-er12.ps1 -GameProcessId 1234 -OutputRoot G:\x2
```

1. Aguarde a confirmação do receptor e pressione Enter no console.
2. Volte ao jogo nos cinco segundos de preparação.
3. Nos cinco segundos de coleta, registre cerca de um segundo normal, aproxime
   a barra da lava e continue por pelo menos um segundo depois do efeito.
4. Aguarde `capture.done` ou `capture.failed` antes de repetir ou fechar o jogo.

Não espere o defeito aparecer para iniciar: não há histórico anterior. Se não
couber, prepare um ponto mais próximo do gatilho e faça outra coleta, preservando
a anterior. Cada execução cria uma pasta exclusiva e pode reutilizar o processo
após a exportação anterior. São esperados aproximadamente 300 quadros a 60 Hz;
o limite é 360 quadros, com orçamento de 512 MiB para o buffer existente.
Não use turbo, rewind, save/load ou pausas durante a janela. Não pressione R
nem mude as configurações durante a coleta.

## Notas e evidências

Preencha `notes.txt`: sala e barra exatas, percurso, posição inicial, movimento,
instante/região/duração do defeito, ocorrência antes/durante/depois da coleta,
EXE normal ou DelayBG2-test, backend, escala, filtros, widescreen, mods, velocidade,
foco e dispositivo usado. Para Bnes, registre versão/configuração e sequência
equivalente com a mesma ROM; não suponha equivalência dos números de quadro.

- `native-XXXXXXXX.ppm` e `capture.tsv` compartilham IDs de chamada do port.
- `row.renderer`: 0=A/legado; 1=B/novo. R do teclado é diferente do R SNES.
- `video_state` e `regs64`: estado observado após desenhar a linha, antes de
  restaurar o VOFS diferido e antes de `RenderEnd`.
- `phase_line`: máscara efetivamente passada ao driver; `04`=BG3 e `06`=BG2+BG3.
  É política, não prova de uma escrita HDMA em cada linha. O scroll consumido
  está em `video_state`. A coleta mantém a política da build; não há experimento.
- O cabeçalho `continuous_BG2_build` identifica a opção compilada no receptor.
  Build normal mantém BG2 contínuo desligado; DelayBG2-test o mantém ligado.
- `consumed_vram`: hashes e perdas, sem payload. `obj_line` inclui hashes de
  OAM/CGRAM e buffer OBJ anterior à composição; não identifica a barra.
- `bg3_sample` tem validade restrita a Mode 1 sem mosaico relevante; `valid=0`
  não prova ausência de BG3. Flags OBJ acumuladas não provam a causa.

O script faz checagens preliminares de quadros/imagens, renderer e chaves da
política. Conferir também chaves dos demais registros e todas as perdas de
PPU/DMA/probes/journals antes de diagnosticar. `capture.done` confirma exportação,
não ausência do defeito ou integridade completa. A apresentação GPU não é
capturada (`presentation=unavailable`); se apenas a janela mostrar o defeito,
será necessário vídeo da janela com referência temporal.

Preserve cada pasta inteira, inclusive falhas, manifestos, notas e marcadores.
Não exporte ROM, tiles brutos ou dumps de memória. Não há captura global de
teclado; os registros R existentes vêm somente dos eventos da janela do jogo.
ER-12 permanece separado do chefe ER-07 e da passagem/porta ER-10.

## Experimento BG3Pair — opcional, ainda sem validação em jogo

A build `Yoshi's Island recomp.ER12-BG3Pair-test.exe` mantém BG2VOFS e BG3VOFS
com atraso contínuo (`06`) e acrescenta atraso de BG3HOFS **somente quando o
mesmo slot HDMA escreve os dois eixos de BG3**. Os dois valores anteriores são
consumidos juntos. Após a linha, o driver repõe os resultados reais de HDMA;
`RenderEnd` continua restaurando o estado guest. Inicialização HDMA, latches,
controles, BG2HOFS e algoritmo da PPU não recebem uma nova política.

`YI_ER12_BG3_PAIR_TEST` é OFF por padrão e exige `YI_DELAY_BG2_TEST=ON` nesta
variante, para preservar o controle anterior. O ajuste é contínuo desde a
abertura do novo EXE, inclusive fora da janela de captura. Não é correção
confirmada nem deve ser promovido ao padrão antes da comparação visual.

Feche o EXE anterior e abra somente a nova variante em `G:\x2`. Prepare a
barra rosa já visível, antes do momento que deseja comparar; não use a passarela
alaranjada como objeto alvo. Não pressione R durante a coleta. Execute:

```powershell
.\scripts\capture-render-er12.ps1 -BG3PairTest
```

Mantém 5 segundos de preparação e 5 segundos de coleta. Mostre as duas pontas
com os mesmos ângulos/percurso das capturas anteriores. Anote se as extensões
sumiram, persistiram ou mudaram; registre também qualquer deslocamento novo.
O script não escolhe A/B; o renderer vem de `row.renderer`. Uma comparação com
o outro renderer deve ser outra pasta, com a troca manual antes de armar.

O parâmetro apenas exige a build e a resposta certas: não ativa ou desativa
política no processo. Sem ele, o script continua aceitando as builds anteriores.

- `phase_line`: política vertical mantida em `06`.
- `phase_h_line`: frame, linha, política horizontal solicitada (`04`), máscara
  horizontal efetivamente aplicada (`04` ou `00`). `00` é esperado quando o
  slot não escreve ambos os eixos; o script exige ao menos uma linha aplicada.
- `video_state`: scroll efetivamente consumido, que permite verificar se o par
  corresponde à mesma transferência anterior.
- O marcador `yi-render-er12-bg3-pair-test-v1` deve estar no EXE e na resposta
  `capture.armed`; ambos são conferidos com `-BG3PairTest`.

Preserve o EXE ER12-DelayBG2-test anterior e suas capturas como controle. Este
experimento não reabre ER-07/ER-10 nem certifica outras cenas.
