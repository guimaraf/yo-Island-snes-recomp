# Coleta ER-05 com entradas

Instrumentação passiva `yi-render-input-v1`. Não corrige ER-05 e não altera
entrada, renderer, HDMA ou ER-03. Sem build ou execução do jogo pelo agente.

## Build e teste local

Feche a instância antiga antes de abrir a nova build. No ambiente de compilação
que você já utiliza, na raiz do projeto, execute você mesmo:

```powershell
cmake --build build-ninja --config Release --target SuperMarioWorld2YoshiSIslandSNESRecomp -j
```

O cache atual aponta `SNESRECOMP_ROOT` para `modules/snesrecomp`; a alteração
em `runner/src/desktop/host_main.c` também precisa entrar nessa build.
Use seu procedimento habitual para disponibilizar a build em `G:\x2`.
O agente não compila nem copia executáveis. Abra a nova instância.

Execute a variante diretamente do projeto (não precisa substituir o script
original em `G:\x2\scripts`):

```powershell
& ".\scripts\capture-render-er05-inputs-10s.ps1" -OutputRoot 'G:\x2' -BoxRegion 0,0,256,224 -Scene 'ER-05; abertura; B novo primeiro, A legado depois; avanço somente após seta'
```

Se houver múltiplas instâncias, acrescente `-GameProcessId` com o PID correto.
A variante exige o marcador no EXE e a confirmação `capture.armed` do processo
em até 30 segundos; isso confirma a capacidade de registro carregada, não uma
identidade criptográfica de todo o código em memória.

1. Prepare a mesma cena da coleta anterior com renderer B/novo (1).
2. Arme pelo script, pressione Enter no console e volte ao jogo. São 5 segundos
   de preparação e 10 segundos de captura, como no original.
3. Faça a primeira abertura no modo B. Pressione A do SNES somente depois da
   seta para baixo. Esses avanços são parte normal do teste.
4. Entre as sequências completas, use a tecla R para mudar para A/legado (0).
   Capture a segunda abertura completa, avançando A somente após a seta.
5. Aguarde `capture.done`. Preserve e forneça a pasta inteira, incluindo imagens,
   `capture.tsv`, `session.json`, `capture.armed`, `notes.txt` e os marcadores.
   Anote a mensagem, a seta, foco e qualquer sequência que tenha ficado cortada.

Não use turbo, frameskip, rewind, run-ahead ou save/load durante a coleta.
Não alterne renderer no meio de uma abertura. Se duas aberturas não couberem,
preserve o resultado incompleto; isso será evidência para ajustar a janela.
A coleta anterior comportou as duas sequências, portanto a duração foi mantida.

## Correlação verificável

`capture.tsv` continua contendo os registros anteriores. Acrescenta:

- `pad`: frame, slot (0 ou 1), counter, previous_counter, previous_valid,
  mask, pressed, released, A_down, A_up. Máscaras em hexadecimal.
- `keyboard_r`: próximo frame da simulação, counter no tratamento SDL,
  pressed (1 pressionada, 0 solta). Repetições de keydown são preservadas.
- `r_summary`: frame, quantidade de eventos R, eventos perdidos por capacidade.

Os bits do runner são B=001, Y=002, Select=004, Start=008, Up=010,
Down=020, Left=040, Right=080, A=100, X=200, L=400, **SNES R=800**.
`keyboard_r` é a **tecla R**, separada do botão SNES R. A configuração pode
remapear a ação da tecla: o renderer efetivo deve ser confirmado pelo campo
renderer de cada `row` (0 legado, 1 novo), não deduzido apenas de um keydown.

A amostra ocorre em `GameRunOneFrame`, depois de `RtlRunFrame` aplicar as
entradas filtradas em `input1_currentState` e `input2_currentState`, antes do
guest executar. O ID é o mesmo de `native-XXXXXXXX.ppm`, `row` e
`host_interval`. Os counters usam SDL_GetPerformanceCounter; a frequência
está no cabeçalho. Segundos relativos = (counter - origem) / frequência.

`pressed` e `released` comparam amostras consecutivas, inclusive com a última
amostra anterior à janela. `previous_valid=0` impede fabricar uma transição
na primeira amostra após início/reset. O timestamp do A é o da amostra que
entregou o novo estado ao guest; o intervalo entre previous_counter e counter
delimita a mudança observada. Não representa o instante físico do contato.
Pressões físicas inteiramente entre polls e não recebidas pela simulação não
aparecem como A lógico. Não identifica dispositivo físico nem prova leitura
de um registrador pelo código da mensagem.

Eventos R são observados no HandleInput existente, somente para SDLK_r;
não há hook global, segundo polling ou coleta de outras teclas. Eles ficam
associados à próxima chamada de simulação, inclusive se houve pausa antes.
Capacidade: 32 eventos R por intervalo entre chamadas; perdas são explícitas.
Nenhuma escrita de arquivo ocorre no callback ou na amostragem; a exportação
usa os slots completos já publicados ao writer pela captura existente.

Só há histórico exportado de entradas para os quadros retidos. Para afirmar
que a caixa aparece antes de qualquer A **na sequência**, é necessário que
a janela inclua o início dessa sequência e não apresente perdas, cortes ou
um A já mantido no primeiro quadro. A preparação de 5 segundos não exporta
imagens nem um histórico completo de entradas.

Na próxima análise, conferir IDs consecutivos, marcadores, máscaras iniciais,
perdas e renderer efetivo; localizar seta/caixa completa/quadro preto nas
imagens e cruzar com A_down/A_up. Coincidência temporal isolada não prova
causa. Comparar ambos os modos antes de propor qualquer correção, que exige
autorização separada.
