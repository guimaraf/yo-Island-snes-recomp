# ER-09 — fase 1-7, coleta passiva

Instrumentação para o usuário executar depois de seu build local atualizado.
Nenhuma correção de renderização foi aplicada nesta etapa. O script não inicia
o jogo, não injeta entradas e não seleciona renderer. Não copie apenas o script
para um executável antigo: o receptor precisa de `yi-render-er09-passive-v1`.

## Duas execuções independentes

1. Abra o jogo normalmente. Não pressione a tecla R. Use a configuração habitual
   e registre backend, escala, widescreen, mods, dispositivo e mapeamento nas notas.
2. Prepare a fase 1-7 antes da região da placa mostrada nas imagens, ainda antes
   do deslocamento. Execute no PowerShell, a partir da pasta do projeto:

   ```powershell
   .\scripts\capture-render-er09.ps1 -Run Padrao -Scenario Placa -OutputRoot G:\x2
   ```

3. O script arma o receptor e espera Enter no console. Após Enter, há cinco
   segundos para retornar ao jogo, seguidos de dez segundos de gravação.
   Dentro da janela, registre aproximadamente um segundo parado, caminhe pelo
   mesmo percurso em direção à placa e à elevação relatada, e continue depois
   do deslocamento. Anote direção, saltos e o instante aproximado do defeito.
   Não espere o defeito para iniciar: esta janela não tem histórico anterior.
4. Se já estiver defeituoso ao chegar à placa, faça outra coleta com
   `-Scenario Entrada`, começando a gravação antes da transição para 1-7.
   Se o trecho não couber em dez segundos, repita de um ponto anterior mais
   próximo. Uma sequência que começa defeituosa não identifica o primeiro quadro.
5. Aguarde `capture.done` ou `capture.failed` antes de fechar o jogo ou repetir.
6. Feche e reabra o mesmo executável com a mesma configuração. Nesta segunda
   execução, pressione a **tecla R do teclado uma vez**, antes de entrar na fase.
   Repita o percurso e a captura:

   ```powershell
   .\scripts\capture-render-er09.ps1 -Run UmaTrocaR -Scenario Placa -OutputRoot G:\x2
   ```

`Padrao` e `UmaTrocaR` são relatos do procedimento, não identificadores de A/B.
Não alterne durante a janela. O botão R do controle SNES é outra entrada.
Não mude controles. Se houver mais de um processo do jogo, informe
`-GameProcessId` com o PID correto. Evite turbo, frameskip, rewind, netplay,
save/load e pausas durante as coletas. Mantenha a mesma configuração em ambas;
largura nativa facilita comparar os caminhos de desenho.

## Arquivos e interpretação

Preserve **cada pasta inteira**, incluindo `session.json`, `notes.txt`,
`capture.tsv`, todos os `native-*.ppm` e os marcadores, mesmo em caso de falha.
Não substitua o EXE enquanto estiver aberto. Seu hash em disco não autentica
uma imagem antiga já carregada. Não exporte ROM, tiles ou dumps de memória.

- `native-XXXXXXXX.ppm` e entradas TSV compartilham o ID da chamada do port.
- `row`: linha de saída 0–223, validade, hash, registros e renderer efetivo
  (`0` = A/legado; `1` = B/novo). O modo padrão não é presumido.
- `video_state`: modo, BGMODE/MOSAIC, scroll dos quatro BGs, BGxSC, máscaras
  main/sub e color math. Os demais registros e janelas estão em `regs64`,
  cujo layout é descrito pelo próprio TSV.
- `consumed_vram`: hashes de 16 páginas da VRAM consultada, campos e perdas.
- `bg3_sample`: sonda restrita a BG3/Mode 1 sem mosaico; `valid=0` não é
  evidência de BG3 ausente. Não identifica o terreno ou o vencedor da composição.
- `dma`, `dma_event` e `hdma_sample`: metadados de transferências e canais.
- `pad` e `keyboard_r`: entradas lógicas SNES e eventos R da janela separados;
  não há monitoramento global do teclado.

A coleta usa os limites existentes: 620 quadros, orçamento de 768 MiB,
8192 eventos PPU e 512 eventos DMA por quadro. Cerca de 600 quadros são esperados
a 60 Hz, mas a contagem depende da execução. `capture.done` significa exportação,
não ausência de perdas ou sucesso visual. A verificação do script é preliminar.

Na análise, conferir IDs consecutivos, imagens presentes, 224 linhas válidas e
únicas por quadro, correspondência de chaves frame/linha e todos os contadores
de perdas antes de comparar o último quadro normal com o primeiro defeituoso.
Separar o movimento das imagens de um movimento real da câmera: scroll PPU é
estado de desenho, não uma posição de câmera ou colisão do jogo autenticada.

O framebuffer final GPU não é capturado (`presentation=unavailable`). Se a imagem
nativa estiver correta enquanto a janela estiver errada, registrar essa diferença
e fornecer vídeo da janela com uma indicação temporal, sem supor correspondência
exata com quadros nativos. Esta etapa não muda o driver, o atraso BG3VOFS ou as
correções anteriores, e não executa build ou testes automaticamente.
