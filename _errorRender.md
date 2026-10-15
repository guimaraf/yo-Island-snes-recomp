# Yoshi’s Island — registro de artefatos de renderização

Registro inicial: 14/10/2026. Projeto: **Super Mario World 2: Yoshi’s Island (USA)**.
Objetivo: manter uma lista de problemas observados, suas evidências e critérios
de validação, para investigar e resolver cada ocorrência separadamente.

Este documento fica na raiz do workspace. As sete imagens ficam em
`Yoshi's Island recomp_errorRender/`, também fora da pasta do port.
As imagens são cópias integrais dos anexos fornecidos pelo usuário, sem
recorte ou edição. A numeração preserva a ordem da mensagem, não necessariamente
a ordem cronológica do vídeo.

## Contexto e limites da evidência

- O usuário confirmou que as piscadas de tela inteira desapareceram na intro
  após substituir o driver personalizado pelo `beam_frame_driver` local.
- A captura `G:\x2\yoshi-trace-20261014-202555` mediu os frames 640–699:
  nenhum quadro completamente preto, uma fronteira V=225 por chamada, nenhum
  descarte registrado e nenhum avanço do clock da CPU durante desenho.
- O relatório dessa execução registrou 4.807 simulações e apresentações em
  aproximadamente 80,35 segundos. A telemetria detalhada cobre somente o
  intervalo configurado; não cobre todos os 80 segundos.
- Os artefatos listados abaixo permanecem abertos. A correção das piscadas não
  demonstra que todos os detalhes de rasterização estejam corretos.
- O usuário também testou no emulador chamado **Bnes** para comparação. Não foram
  fornecidos versão/configuração desse emulador, pares de frames identificados
  como referência/recomp ou números de frame equivalentes. Portanto, não se
  atribui aqui cada captura a um emulador nem se declara comprovada a diferença
  em relação ao original. Preservar a grafia informada; não presumir outro core.
- Frames estáticos mostram a aparência, mas não comprovam intermitência, duração
  ou sequência temporal. Esses aspectos são registrados como relato do usuário.
- As hipóteses abaixo orientam medições; nenhuma delas constitui causa confirmada.
  O Super FX é parte do caminho de execução, não uma explicação automática.

## Lista de acompanhamento

| ID | Cena / sintoma | Evidência | Estado |
| --- | --- | --- | --- |
| ER-01 | Faixa horizontal escura na região inferior do céu/nuvens | Imagens 1 e 2; relato de intermitência | Aberto |
| ER-02 | Corrupção em blocos durante aproximação/saída do inimigo | Imagem 3 como comparação e imagem 4 afetada | Aberto |
| ER-03 | Primeira linha da tela título com pixels de cores discrepantes | Imagem 5; relato de intermitência | Aberto |
| ER-04 | Linha horizontal clara fixa atrás das montanhas no título | Imagem 5; relato de persistência | Aberto |
| ER-05 | Caixa de texto completa aparece antes da animação de abertura | Imagens 6 e 7; ordem temporal descrita pelo usuário | Aberto |

## ER-01 — faixa escura no céu da intro

**Observação nas imagens:** há uma descontinuidade horizontal próxima à parte
inferior da área do cenário, acima da moldura inferior. Na imagem 1 ela aparece
como um traço preto entre regiões de nuvens; na imagem 2 há uma faixa com mudança
de tonalidade/padrão, incluindo partes mais escuras e pretas. A moldura e as
legendas continuam presentes: é um defeito localizado, diferente dos apagamentos
completos já tratados.

**Relato temporal:** a linha pisca durante quase toda a apresentação desse
cenário em movimento. Às vezes escurece apenas alguns pixels.

**Hipóteses a distinguir:** transição de estado raster/HDMA na altura errada;
janela, color math ou mudança de camada; leitura de tiles/paleta em um instante
incompatível com o restante da linha. Não há dados suficientes para escolher.

**Próxima evidência:** frames consecutivos com e sem faixa, na mesma cena,
identificados por execução e frame; estado aplicado, hashes das linhas e escritas
de display ao redor da faixa. Comparar também com uma sequência de referência.

**Critério de resolução:** movimento completo da cena sem a faixa intermitente
adicional, preservando as transições que existirem na referência.

![Imagem 1 — traço preto atrás das nuvens](<Yoshi's Island recomp_errorRender/imagem01_YoshisIsland_errorRender.png>)

![Imagem 2 — faixa com pixels escurecidos e partes pretas](<Yoshi's Island recomp_errorRender/imagem02_YoshisIsland_errorRender.png>)

## ER-02 — corrupção durante transformação do inimigo

**Observação nas imagens:** a imagem 3 mostra o inimigo pequeno e reconhecível no
cenário. Na imagem 4, regiões grandes do cenário contêm blocos e fragmentos
gráficos, incluindo partes que lembram o inimigo. A legenda e a moldura continuam
legíveis. A corrupção não deve ser reduzida a simples desaparecimento de sprites.

**Relato temporal:** o inimigo surge ao fundo, cresce e o último frame dessa
animação parece corromper seus tiles. A imagem 3 foi apontada como não afetada
quanto à aparência do inimigo; isso não atesta ausência de ER-01 nesse cenário.

**Hipóteses a distinguir:** conteúdo gráfico atualizado durante uma transição,
relação temporal entre produção pelo GSU, transferência e leitura de VRAM,
ou estado de endereçamento/modo do renderer. Não há prova de que a imagem 4
contenha exclusivamente tiles do inimigo nem de que a execução do GSU seja a causa.

**Próxima evidência:** sequência curta antes/durante/depois do evento; hashes por
região de memória, metadados de transferências e cadência dos trabalhos GSU,
sem exportar conteúdo da ROM ou dumps de tiles. Correlacionar com o frame nativo.

**Critério de resolução:** animação inteira e transição final sem fragmentação
adicional em relação à referência.

![Imagem 3 — inimigo reconhecível antes do defeito relatado](<Yoshi's Island recomp_errorRender/imagem03_YoshisIsland_errorRender.png>)

![Imagem 4 — fragmentação em blocos na área do cenário](<Yoshi's Island recomp_errorRender/imagem04_YoshisIsland_errorRender.png>)

## ER-03 e ER-04 — duas linhas distintas na tela título

**ER-03, observação:** há uma linha de pixels discrepantes no limite superior da
imagem do jogo. O usuário relata que ela alterna com uma linha normal.

**ER-04, observação:** uma linha fina clara/amarelada cruza o fundo na região do
horizonte, passando atrás das montanhas. O usuário relata que é fixa e não pisca.

Tratar os dois casos separadamente: posição, persistência e aparência diferentes
não justificam presumir uma causa única.

**Hipóteses para ER-03:** inicialização da primeira linha, origem/pitch do
framebuffer, estado raster aplicado à primeira linha ou conteúdo atualizado.
**Hipóteses para ER-04:** transição raster de camadas/paleta/color math ou limite
de janela. Ainda é necessário excluir um detalhe presente na referência.

**Próxima evidência:** pares do título com ER-03 presente/ausente e referência
equivalente para ER-04; comparar linha nativa com imagem apresentada para separar
renderização de escala/apresentação. Não inferir a scanline SNES exata a partir
da posição numa captura escalada.

**Critérios de resolução:** ER-03 sem pixels residuais intermitentes; ER-04 com
continuidade de cor compatível com a referência, sem remover detalhe legítimo.

![Imagem 5 — primeira linha discrepante e linha clara no horizonte](<Yoshi's Island recomp_errorRender/imagem05_YoshisIsland_errorRender.png>)

## ER-05 — conteúdo da caixa de texto aparece antecipadamente

**Observação nas imagens:** a imagem 6 contém uma caixa verde grande com o texto
já visível. A imagem 7 contém um retângulo preto grande e somente uma região
verde pequena no centro, com fragmentos de letras. Isoladamente, cada imagem
poderia pertencer a uma etapa da animação; o defeito temporal depende do relato
e da futura sequência de frames.

**Relato temporal:** o comportamento esperado é a janela preta crescer primeiro,
seguida da expansão da caixa colorida e texto. No recomp, a caixa completa e o
texto aparecem logo no primeiro frame, somem e então a abertura progride. A
imagem 7 representa uma fase posterior em que a parte colorida começa a crescer.

**Hipótese sugerida pelo usuário:** imagem já presente no buffer. Registrar como
hipótese, não conclusão. Outras possibilidades incluem estado de janela/máscara
de um instante diferente do conteúdo, ordenação de uploads e replay, ou linha
não redesenhada. Não aplicar limpeza de buffer ou latch indiscriminadamente.

**Próxima evidência:** sequência contínua desde antes da abertura, incluindo o
primeiro frame em que a caixa aparece, o desaparecimento e o crescimento;
acompanhar janelas, camadas, transferências e hashes de linhas. Comparar com a
mesma abertura na referência.

**Critério de resolução:** ausência do aparecimento antecipado, com ordem e
geometria de abertura compatíveis com a referência.

![Imagem 6 — caixa completa apontada como aparição antecipada](<Yoshi's Island recomp_errorRender/imagem06_YoshisIsland_errorRender.png>)

![Imagem 7 — área preta com região colorida pequena durante abertura](<Yoshi's Island recomp_errorRender/imagem07_YoshisIsland_errorRender.png>)

## Observações de gameplay e limites de generalização

O usuário relata que a corrupção de abertura de texto não ocorreu nas caixas
de gameplay e que não notou corrupção na tela de pause. Manter esses casos como
controles de comparação, sem concluir que todas as caixas, fases ou pausas foram
validadas. O escopo visual de ER-05 está nas transições de texto da apresentação
observadas pelo usuário.

## Ordem sugerida de investigação

1. **ER-03:** delimitar a primeira linha, por ser espacialmente pequena e fácil
   de comparar no framebuffer nativo e na apresentação.
2. **ER-01 e ER-04:** medir separadamente as faixas horizontais; compartilhar
   metodologia de observação raster, sem presumir que compartilham a causa.
3. **ER-02:** capturar a transição do inimigo e a coerência de uploads/conteúdo.
4. **ER-05:** capturar a ordem de abertura de janela versus disponibilização do
   conteúdo, usando as caixas de gameplay sem erro relatado como comparação.

Esta ordem é uma proposta de diagnóstico, não prioridade obrigatória. Se um
evento se reproduzir com maior facilidade, começar pela evidência mais completa.

## Dados mínimos para cada nova coleta

- ID do erro; executável/revisão; configurações; cena e sequência de inputs.
- Origem de cada imagem: recomp ou emulador de referência, versão e configurações.
- Frame anterior, afetado e posterior, com contador ou timestamp do vídeo.
- Região e duração do defeito; se ocorre sempre ou depende da execução.
- Captura curta configurada para a cena: o intervalo 640–699 usado na intro
  inicial não cobre automaticamente título, texto posterior ou gameplay.

Preservar as coletas anteriores em `G:\x2`. Não exportar ROM nem conteúdo bruto
de memória proprietária para este workspace. As imagens deste registro foram
fornecidas explicitamente pelo usuário para documentar os sintomas.

## Plano para retomada futura — ainda não implementado

**Situação ao encerrar esta etapa:** somente documentação atualizada. Não há
captura por tecla, histórico circular de imagens nem novos logs de transferências
implementados por este plano. Não há testes agendados ou execução automática.
O usuário fará os testes em outra ocasião.

### 1. Preservar a base que eliminou os apagamentos

- Manter `beam_frame_driver` como base experimental validada na intro. Não
  reintroduzir IRQ guest no desenho, um segundo beam ou orçamento de frame
  independente da fronteira do campo.
- Antes de editar no futuro, reler as instruções atuais, este documento,
  `src/game_rtl.c` e `snesrecomp/docs/FRAME_MODEL_HOSTS.md` dentro do port.
  Conferir os fontes locais e os pins: o pin declarado anteriormente não pôde
  ser autenticado por metadados Git próprios da cópia local do framework.
- Registrar o SHA-256 do executável de referência, configuração e captura usada.
  O EXE copiado após a mudança de driver tinha SHA-256
  `3320A1C1391DE0A67F711C1A0B48AEA84161E0D9E81177660309CD667B99D095`.
  Conferir novamente; não presumir que o arquivo permaneceu igual.
- Preservar `G:\x2\yoshi-trace-20261014-201927` (antes) e
  `G:\x2\yoshi-trace-20261014-202555` (depois). Não substituir essas pastas.

### 2. Ampliar a captura sem mudar o comportamento do jogo

**Proposta:** um modo opt-in com histórico circular limitado em memória e
acionamento por tecla ainda a definir, sem conflito com controles ou atalhos.
O acionamento deve somente marcar a captura: não pausar, resetar, avançar frames
manualmente ou escrever no estado guest.

- Janela inicial proposta: **8 frames anteriores e 16 posteriores ao frame
  marcado**, além do próprio frame do acionamento (25 frames no total).
  Permitir ajustar o intervalo, com teto explícito de memória, frames e eventos.
- Guardar imagens nativas e telemetria no mesmo histórico; registrar apenas logs
  antes do acionamento não permitiria recuperar a aparência dos frames passados.
- Calcular o limite de memória a partir da resolução/pitch efetivos e da
  capacidade dos eventos. Pré-alocar buffers; evitar alocação e escrita em disco
  por pixel ou evento. Descarregar o lote de forma controlada e registrar qualquer
  perda, sem bloquear indefinidamente o jogo.
- Exportar um lote por acionamento, com limite de lotes por sessão. Se o buffer
  ou a janela ainda não estiver pronto, informar isso no resultado; não produzir
  uma sequência silenciosamente incompleta.
- A reação humana pode exceder oito frames. Para ER-02 e ER-05, localizar antes
  o timestamp no vídeo e usar uma janela programada na repetição da cena, ou
  aumentar o histórico dentro do orçamento. Uma tecla tardia não é prova de
  ausência do defeito.
- Manter o modo de intervalo fixo existente como alternativa para cenas
  reproduzíveis desde o boot. Não reutilizar 640–699 para uma cena posterior
  sem primeiro identificar seu intervalo.

**Saída proposta por lote, somente em nova subpasta de `G:\x2`:**

| Arquivo / grupo | Conteúdo esperado |
| --- | --- |
| Manifesto | ID ER, versão do esquema, hash do EXE, configuração, resolução/pitch/formato, cena, origem dos contadores, frame marcado e limites/perdas |
| Imagens nativas | Sequência antes/durante/depois, nomeada pelo mesmo frame dos logs; conteúdo visual da tela, sem dumps de tiles ou ROM |
| Imagens apresentadas | Quando disponíveis sem reexecutar guest, associadas explicitamente ao frame simulado e à apresentação |
| Estado por linha | Estado efetivo, hashes e contagens de pixels; linha de desenho e linha de framebuffer identificadas |
| Eventos | Escritas de display, metadados de DMA/HDMA e amostras/trabalhos GSU, com clocks, fase, beam e origem |
| Resumo | Frames presentes/ausentes, acionamentos, descartes e instruções para correlação |

Não exportar payload de VRAM/OAM/CGRAM, SRAM, WRAM ou código da ROM. Para
conteúdo gráfico, usar hashes de regiões e metadados de destino/tamanho. Hashes
globais detectam mudanças, mas não identificam sozinhos o tile responsável.

### 3. Medições que precisam acompanhar as imagens

- **Sincronização:** frame do port, campo PPU, apresentação, linha de desenho,
  V/H, master clock CPU e beam. Marcar a fase de cada amostra; não confundir o
  beam estacionado durante replay com a scanline que originou a escrita guest.
- **Estado PPU:** INIDISP, modo, camadas main/sub, scroll, parâmetros Mode 7,
  seleção/lógica/limites de janelas, máscaras e color math. A janela de
  registradores da instrumentação atual já contém vários desses dados; exportar
  nomes legíveis para evitar depender da interpretação manual dos 64 bytes.
- **Estado realmente consumido:** aproveitar o observador após a linha já
  existente. Se necessário, acrescentar observação antes do desenho e dos limites
  de janela calculados, diferenciando configuração de janela e resultado efetivo.
- **Framebuffer:** hash por linha e por região de interesse, pixels não pretos,
  dimensões válidas, pitch e indicação de linha redesenhada. Comparar o nativo
  com a apresentação para localizar ER-03 sem mascarar o defeito.
- **DMA/HDMA:** canal, destino, tamanho, início/fim quando o modelo permitir,
  enable e avanço da tabela. Registrar limites de precisão do timing por byte;
  não inventar timestamps que a API não fornece. Correlacionar mudanças de hashes
  com transferências e consumo pelo renderer.
- **GSU:** início/fim dos trabalhos e clocks, progresso de instruções, SFR/IRQ e
  configuração relevante. Consultar `superfx_job_log` local antes de adicionar
  outra infraestrutura. Não exportar opcodes ou conteúdo de memória.
- **Integridade:** distinguir descarte do journal, descarte da captura e estouro
  de outros rings; documentar claramente qualquer informação indisponível.

### 4. Mapa de implementação a confirmar antes das edições

Todos os caminhos abaixo são relativos a `Yoshi's Island recomp/`. São candidatos
para a futura implementação, não arquivos modificados nesta etapa. Inspecionar
as APIs e anunciar os caminhos exatos efetivamente necessários antes de editar.

| Caminho | Papel previsto |
| --- | --- |
| `src/game_rtl.c` | Captura delimitada, correlação por frame/linha e integração do observador existente |
| `snesrecomp/runner/src/desktop/host_main.c` | Possível acionamento e observação da apresentação; confirmar primeiro extensão existente para evitar alteração desnecessária |
| `snesrecomp/runner/src/debug_server.c` | Consultar recursos locais de imagens consecutivas, `ppu_lines` e `ppu_window`; conferir disponibilidade na build e retenção por frame antes de depender deles |
| `snesrecomp/runner/src/snes/ppu.c` e `ppu.h` | Somente se faltarem observadores de estado/linhas; manter hooks opcionais e sem mutação guest |
| `snesrecomp/runner/src/snes/dma.c` e `superfx.c` | Consultar logs existentes; editar apenas se faltarem metadados necessários e após delimitar o escopo |
| `tools/flicker-trace.md` | Atualizar instruções, esquema dos arquivos e comandos quando o recurso estiver implementado |

Não presumir que consultas de estado da última linha/frame permitam recuperar
um frame antigo. Não usar injeção de estado ou renderização que execute guest
como substituto de uma captura passiva.

### 5. Sessões de teste futuras

1. **Validar a coleta:** confirmar que acionamento, imagens, IDs e logs concordam;
   conferir limites e perdas. Repetir o trecho da intro sem os apagamentos como
   controle. A coleta não deve modificar clocks guest, IRQs ou imagem.
2. **Título — ER-03:** capturar primeira linha normal e afetada, incluindo linhas
   vizinhas, na saída nativa e apresentada. Investigar origem, pitch, área válida
   e redraw somente se a divergência apontar para esse caminho.
3. **Céu — ER-01:** capturar a faixa presente/ausente e comparar o estado aplicado
   nas linhas vizinhas. Procurar a primeira diferença correspondente à faixa.
4. **Horizonte — ER-04:** confirmar primeiro a diferença em relação à referência;
   medir separadamente de ER-01, mesmo que os instrumentos sejam compartilhados.
5. **Inimigo — ER-02:** capturar crescimento e transição final. Cruzar alterações
   de conteúdo/endereço com uploads e trabalhos GSU, sem concluir causalidade
   somente porque dois eventos ocorreram no mesmo frame.
6. **Texto — ER-05:** iniciar antes da abertura. Capturar a aparição completa,
   desaparecimento e crescimento. Comparar máscara efetiva e conteúdo disponível.
   Repetir uma caixa de gameplay sem erro relatado e o pause como controles.
7. **Gameplay por observação:** avançar normalmente, registrando cena e inputs;
   verificar ausência de retorno dos apagamentos e de regressões de input,
   áudio, caixas de texto e pause. Ausência de defeito observado em uma sessão
   não equivale a validação de todo o jogo.

Manter frameskip, turbo, run-ahead, rewind, reset e save/load desativados durante
a captura comparativa. Inputs normais são necessários para título/gameplay;
somente a reprodução inicial da intro exige ficar sem input. Manter a janela
visível e anotar qualquer mudança de foco ou configuração durante o intervalo.

Na referência Bnes, registrar versão, configurações e sequência equivalente.
Alinhar pelo estágio da animação e pelos inputs, não apenas pelo número absoluto
de frame desde o boot. Comparar resolução nativa quando possível; escala e
proporção da apresentação podem deslocar coordenadas aparentes.

### 6. Como decidir e validar cada correção

Para cada ID, registrar no próprio documento: evidência que localizou a
divergência, hipótese testada, caminhos alterados, build realmente executada,
captura antes/depois e resultado observado pelo usuário.

Aplicar uma alteração delimitada por vez, preservando um executável de referência
e as coletas anteriores. Não limpar buffers, prender VRAM em um latch, ajustar
clocks ou ocultar linhas sem evidência que justifique a mudança. Não usar filtro
visual para disfarçar o erro.

Marcar **resolvido no cenário testado** somente após repetir a sequência completa
do ID e conferir que a alteração não reintroduziu apagamentos ou afetou os
controles de comparação. Registrar separadamente build bem-sucedida, inspeção
estática e teste visual; uma não substitui as outras.

### 7. Ponto de retomada

A próxima ação técnica é implementar e verificar a captura curta com imagens e
telemetria sincronizadas, começando por ER-03. Antes disso, confirmar se o usuário
prefere iniciar por outro ID e seguir a autorização/instruções vigentes para
editar, compilar e copiar o EXE. Esta atualização documental não executa nem
autoriza automaticamente essas etapas futuras.

## Histórico

| Data | Registro |
| --- | --- |
| 14/10/2026 | Piscadas de tela inteira eliminadas na intro segundo teste do usuário e intervalo instrumentado; não equivalem à resolução dos artefatos locais. |
| 14/10/2026 | Inclusão de ER-01 a ER-05 com sete imagens fornecidas; nenhuma correção adicional ou build executada para este documento. |
| 14/10/2026 | Links ajustados para `Yoshi's Island recomp_errorRender/`; plano futuro de instrumentação, sessões por erro e critérios de validação documentados. Somente este Markdown foi alterado; implementação e testes pendentes. |
