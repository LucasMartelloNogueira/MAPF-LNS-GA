# Spec: Paralelização do Genetic Algorithm e cálculo de makespan no MAPF-LNS

## Auditoria

- created_at: 2026-07-11 13:47:13 -0300
- author: Lucas Martello Nogueira
- last_updated: 2026-07-16 20:31:43 -0300
- ai_model: GPT-5.4

## Objetivo

Implementar uma nova estratégia de destruição chamada `GENETIC_ALGO_PARALLEL` no MAPF-LNS, baseada na estratégia genética já existente, mas com avaliação paralela do fitness da população em múltiplas threads. A implementação também deve adicionar a métrica global `makespan`, definida neste projeto como o maior atraso entre todos os agentes.

Além disso, a implementação deve corrigir o fluxo atual do algoritmo genético sequencial para que os parâmetros configurados via CLI sejam passados explicitamente no `run()`.

## Fonte do escopo

Este spec foi elaborado a partir de [specs/2-parallel-genetic-algorithm-makespan/plan.md](specs/2-parallel-genetic-algorithm-makespan/plan.md).

## Escopo

- Adicionar opção de CLI `--num_threads` com valor padrão `1`.
- Adicionar o valor `GeneticAlgoParallel` à opção `destoryStrategy`.
- Adicionar `GENETIC_ALGO_PARALLEL` ao enum de heurísticas de destruição.
- Adicionar `makespan` à classe `LNS`.
- Adicionar `num_threads` à classe `LNS`.
- Implementar `generateNeighborByGeneticAlgorithmParallel`.
- Chamar explicitamente os parâmetros do GA sequencial e paralelo a partir de `run()`.
- Reduzir o custo do GA sequencial eliminando reavaliações redundantes de fitness por geração.
- Calcular `makespan` durante a execução do `LNS`.
- Persistir `makespan` no CSV de resultados.
- Ajustar o desempate aleatório do solver para uma estratégia segura em ambiente multi-thread usando seed derivada do id da thread.
- Manter `GeneticAlgoParallel` fora do ALNS.

## Fora de escopo

- Melhorias agressivas de performance além do necessário para obter corretude da execução paralela. Isso exclui, nesta entrega, otimizações como cache sofisticado de `SpaceTimeAStar` por thread, pool de threads, reuso complexo de estruturas de busca, redesign da avaliação de fitness para reduzir cópias de `PathTable` e ajustes finos focados em escalar melhor para populações grandes. Se alguma otimização for indispensável para a corretude mínima da execução paralela, ela pode entrar; se for apenas ganho de desempenho, fica para uma iteração posterior.
- Mudanças nas demais heurísticas de destruição além do necessário para preservar compatibilidade com ALNS. Isso significa que a feature pode ajustar enumeração, contagem, indexação e integração mínima para impedir que `GeneticAlgoParallel` entre no ALNS por acidente, mas não deve aproveitar esta entrega para redesenhar `RandomWalk`, `Intersection`, `Random`, recalibrar pesos adaptativos, mudar critérios de seleção ou alterar o comportamento histórico das heurísticas existentes.
- Redesenho completo do algoritmo genético existente.
- Refatoração ampla de toda a infraestrutura de geração aleatória do projeto.

## Requisitos funcionais

### RF1. CLI para paralelização

O executável deve aceitar uma nova opção:

```text
--num_threads
```

Regras:

- valor padrão igual a `1`;
- o valor representa a quantidade de threads usadas na avaliação paralela do fitness;
- o valor deve ser encaminhado ao construtor de `LNS`.

### RF2. Nova estratégia de destruição

A CLI deve aceitar:

```text
--destoryStrategy GeneticAlgoParallel
```

Regras:

- o construtor de `LNS` deve traduzir essa string para `GENETIC_ALGO_PARALLEL`;
- a estratégia paralela deve ficar disponível apenas por seleção explícita;
- a estratégia paralela não deve ser sorteada pelo ALNS.

### RF3. Estado adicional em `LNS`

A classe `LNS` deve passar a armazenar:

- `int makespan = -1;`
- `int num_threads = 1;`

### RF4. Chamada explícita dos parâmetros do GA

No método `run()`, o caso `GENETIC_ALGO` deve chamar `generateNeighborByGeneticAlgorithm` passando explicitamente:

- `ga_population_size`
- `ga_num_generations`
- `ga_mutation_rate`

No método `run()`, o caso `GENETIC_ALGO_PARALLEL` deve chamar `generateNeighborByGeneticAlgorithmParallel` passando explicitamente:

- `ga_population_size`
- `ga_num_generations`
- `ga_mutation_rate`
- `num_threads`

### RF5. Novo método paralelo

Implementar o método:

```cpp
bool generateNeighborByGeneticAlgorithmParallel(
    int population_size = -1,
    int num_generations = -1,
    double mutation_rate = -1,
    int num_threads = -1);
```

Regras:

- o método deve manter o mesmo contrato de retorno do método sequencial;
- o método deve usar o mesmo pipeline evolutivo do GA sequencial;
- a principal diferença deve ser a avaliação paralela do fitness da população;
- a seleção do melhor indivíduo final deve preencher `neighbor.agents`.

### RF5.1. Fluxo otimizado do GA sequencial

O método `generateNeighborByGeneticAlgorithm` deve reduzir chamadas redundantes de `evaluateFitness`.

Regras:

- a avaliação de fitness deve ocorrer exatamente uma vez por iteração do loop evolutivo;
- a avaliação deve acontecer no início de cada iteração;
- a última iteração deve executar apenas a fase de avaliação;
- o índice do indivíduo mais apto deve ser preservado a partir de `fitness_scores` para uso ao final do método;
- após o loop, `neighbor.agents` deve ser preenchido diretamente com `population[best_idx]`, sem nova avaliação final.

### RF5.2. Crossover do GA sequencial

O crossover do GA sequencial deve operar sem reavaliar a população dentro da mesma geração.

Regras:

- os 4 indivíduos mais aptos devem gerar 4 filhos com os 4 indivíduos menos aptos;
- cada filho deve substituir diretamente um dos 4 indivíduos menos aptos;
- essa etapa não deve chamar `evaluateFitness`.

### RF5.3. Mutação do GA sequencial

A mutação do GA sequencial deve operar sobre a população corrente sem reavaliação intermediária.

Regras:

- a mutação pode ocorrer em todos os indivíduos exceto no indivíduo mais apto da geração;
- essa etapa não deve chamar `evaluateFitness`.

### RF6. Corretude da avaliação paralela

A avaliação do fitness em paralelo deve evitar compartilhamento inseguro de estado mutável.

Regras:

- cada avaliação deve operar sobre uma cópia local de `PathTable`;
- a avaliação paralela não deve reutilizar diretamente `agents[id].path_planner`;
- cada avaliação deve criar solver local por agente quando precisar chamar `findOptimalPath`;
- a implementação deve priorizar corretude antes de otimizações de cache ou reuso.

### RF7. RNG seguro para desempate

O desempate aleatório do solver não deve usar `rand()` global durante a execução paralela.

Regras:

- implementar função auxiliar para desempate pseudoaleatório em `inc/SingleAgentSolver.h` ou equivalente;
- a seed deve ser derivada de `std::this_thread::get_id()`;
- a implementação deve usar `std::hash<std::thread::id>` para converter o id em valor numérico;
- a seed deve ser combinada com dados do nó para evitar repetição trivial dentro da mesma thread;
- a função resultante deve substituir o uso atual de `rand()` apenas nos pontos necessários ao desempate do solver.

### RF8. Cálculo de makespan

O `makespan` deve ser calculado como:

```text
max(agent.getNumOfDelays())
```

Regras:

- calcular após a obtenção da solução inicial;
- recalcular ao fim de cada iteração em que a solução corrente for mantida ou atualizada;
- armazenar o valor no atributo `makespan`.

### RF9. Persistência do makespan

O método `writeResultToFile` deve incluir:

- nova coluna `makespan` no cabeçalho;
- valor de `makespan` na linha de saída.

Regras:

- a lógica existente de migração de cabeçalho CSV deve continuar funcionando;
- arquivos antigos devem continuar legíveis após a atualização do cabeçalho.

### RF10. Compatibilidade com ALNS

`GENETIC_ALGO_PARALLEL` não deve ser integrado ao sorteio adaptativo.

Regras:

- revisar o uso de `DESTORY_COUNT` no dimensionamento de `destroy_weights`;
- revisar `chooseDestroyHeuristicbyALNS()` para garantir que apenas as heurísticas já adaptativas sejam escolhidas;
- a adição da nova estratégia não deve alterar o comportamento do ALNS quando a estratégia explícita paralela não for usada.

## Requisitos não funcionais

### RNF1. Prioridade de entrega

A primeira versão deve priorizar corretude da abordagem paralela. Escalabilidade e micro-otimizações só devem ser tratadas depois que a solução estiver correta e estável.

### RNF2. Compatibilidade com o codebase

A implementação deve permanecer compatível com o padrão C++14 já definido no projeto.

### RNF3. Dependências

Não adicionar bibliotecas externas para paralelização. Usar apenas STL e, se necessário, link explícito com `Threads::Threads` no `CMakeLists.txt`.

### RNF4. Mudanças mínimas e localizadas

As alterações devem se concentrar em:

- `src/driver.cpp`
- `inc/LNS.h`
- `src/LNS.cpp`
- `inc/SingleAgentSolver.h`
- `CMakeLists.txt`, se necessário
- documentação em `docs/`

## Arquivos a alterar

- `src/driver.cpp`
- `inc/LNS.h`
- `src/LNS.cpp`
- `inc/SingleAgentSolver.h`
- `CMakeLists.txt`, se necessário

## Arquivos a criar

- `specs/2-parallel-genetic-algorithm-makespan/spec.md`
- `docs/parallel-ga-makespan.md`

## Plano de implementação

### Etapa 1. Atualizar a CLI

Alterar `src/driver.cpp` para:

- adicionar `num_threads` em `boost::program_options`;
- atualizar a descrição textual de `destoryStrategy` para incluir `GeneticAlgoParallel`;
- passar `vm["num_threads"].as<int>()` para o construtor de `LNS`.

### Etapa 2. Atualizar a interface de `LNS`

Alterar `inc/LNS.h` para:

- inserir `GENETIC_ALGO_PARALLEL` no enum;
- adicionar `makespan` entre os atributos públicos de estatística;
- adicionar `num_threads` entre os atributos internos de configuração;
- atualizar a assinatura do construtor;
- declarar `generateNeighborByGeneticAlgorithmParallel`.

### Etapa 3. Atualizar o construtor de `LNS`

Alterar `src/LNS.cpp` para:

- receber `num_threads`;
- armazenar o valor com limite inferior de `1`;
- reconhecer `destory_name == "GeneticAlgoParallel"`;
- manter o comportamento atual para as demais estratégias.

### Etapa 4. Corrigir o fluxo do GA no `run()`

Alterar `src/LNS.cpp` para:

- chamar explicitamente `generateNeighborByGeneticAlgorithm(ga_population_size, ga_num_generations, ga_mutation_rate)`;
- adicionar o caso `GENETIC_ALGO_PARALLEL` chamando `generateNeighborByGeneticAlgorithmParallel(ga_population_size, ga_num_generations, ga_mutation_rate, num_threads)`.

### Etapa 5. Implementar avaliação paralela da população

Implementar em `src/LNS.cpp` a avaliação da população usando `std::thread`.

Regras de implementação:

- limitar o número efetivo de workers a `min(num_threads, population.size())`;
- distribuir intervalos contíguos de indivíduos por thread;
- armazenar fitness em vetor pré-alocado indexado por posição;
- aguardar `join()` de todas as threads antes de ordenar por fitness.

### Etapa 5.1. Otimizar o fluxo do GA sequencial

Alterar `generateNeighborByGeneticAlgorithm` em `src/LNS.cpp` para:

- executar o loop evolutivo com uma iteração adicional dedicada apenas à avaliação final;
- chamar `evaluateFitness` apenas uma vez por iteração, sempre na etapa inicial de avaliação;
- registrar `best_idx` a partir de `fitness_scores.front().second` após a ordenação;
- usar os 4 melhores indivíduos cruzando com os 4 piores e substituir diretamente os 4 piores pelos filhos gerados;
- permitir mutação em toda a população exceto no melhor indivíduo da geração;
- evitar qualquer reavaliação após crossover, após mutação e após o término do loop.

### Etapa 6. Garantir isolamento thread-safe do solver

Adaptar a avaliação de fitness em `src/LNS.cpp` para:

- trabalhar sobre cópias locais de `PathTable`;
- usar instâncias locais de `SpaceTimeAStar`;
- evitar leitura e escrita concorrentes sobre o estado interno dos `path_planner` armazenados em `agents`.

Se necessário, criar helper separado para avaliação thread-safe, por exemplo:

```cpp
int evaluateFitnessThreadSafe(const vector<int>& individual) const;
```

ou helper equivalente sem alterar a semântica do método já existente.

### Etapa 7. Ajustar RNG do solver

Alterar `inc/SingleAgentSolver.h` para substituir o desempate baseado em `rand()` por helper pseudoaleatório seeded com id da thread.

Direção mínima esperada:

- incluir `<thread>` e `<random>` se necessário;
- obter hash do id da thread;
- combinar a seed com campos relevantes do nó;
- retornar um bit ou valor booleano para desempate.

### Etapa 8. Calcular e manter `makespan`

Adicionar helper dedicado em `LNS`, se necessário, por exemplo:

```cpp
void updateMakespan();
```

Esse helper deve:

- percorrer `agents`;
- calcular `max(agent.getNumOfDelays())`;
- atualizar o atributo `makespan`.

Chamadas mínimas esperadas:

- após a solução inicial;
- após cada iteração relevante do loop principal.

### Etapa 9. Persistir `makespan` no CSV

Atualizar `writeResultToFile` para:

- inserir `makespan` no cabeçalho;
- gravar o valor em cada linha;
- preservar a rotina existente de migração de cabeçalho quando o arquivo já existir.

### Etapa 10. Preservar isolamento do ALNS

Revisar a integração com ALNS para assegurar que `GENETIC_ALGO_PARALLEL` não participe de:

- `destroy_weights.assign(...)`
- sorteio em `chooseDestroyHeuristicbyALNS()`
- indexações derivadas de `DESTORY_COUNT` que assumam participação no conjunto adaptativo

### Etapa 11. Documentar a feature

Criar `docs/parallel-ga-makespan.md` com:

- descrição da estratégia `GeneticAlgoParallel`;
- significado de `num_threads`;
- definição de `makespan` adotada neste projeto;
- observação de que a primeira versão prioriza corretude antes de escalabilidade.

## Snippets de referência

### Chamada do construtor

```cpp
LNS lns(instance, time_limit,
        vm["initAlgo"].as<string>(),
        vm["replanAlgo"].as<string>(),
        vm["destoryStrategy"].as<string>(),
        vm["neighborSize"].as<int>(),
        vm["maxIterations"].as<int>(),
        screen, pipp_option,
        vm["gaPopSize"].as<int>(),
        vm["gaGenerations"].as<int>(),
        vm["gaMutationRate"].as<double>(),
        vm["num_threads"].as<int>());
```

### Chamada do switch em `run()`

```cpp
case GENETIC_ALGO:
    succ = generateNeighborByGeneticAlgorithm(
        ga_population_size,
        ga_num_generations,
        ga_mutation_rate);
    break;

case GENETIC_ALGO_PARALLEL:
    succ = generateNeighborByGeneticAlgorithmParallel(
        ga_population_size,
        ga_num_generations,
        ga_mutation_rate,
        num_threads);
    break;
```

### Helper de makespan

```cpp
void LNS::updateMakespan()
{
    makespan = 0;
    for (const auto& agent : agents)
        makespan = std::max(makespan, agent.getNumOfDelays());
}
```

## Critérios de aceite

- O projeto compila após as alterações.
- `--num_threads` aparece na CLI e usa `1` como padrão.
- `--destoryStrategy GeneticAlgoParallel` é aceito.
- `GENETIC_ALGO_PARALLEL` é reconhecido pelo construtor de `LNS`.
- `run()` passa explicitamente os parâmetros do GA sequencial.
- `run()` passa explicitamente os parâmetros do GA paralelo.
- A avaliação paralela não reutiliza o estado mutável de `agents[id].path_planner`.
- O desempate do solver deixa de depender de `rand()` global nos pontos necessários à execução paralela.
- `GeneticAlgoParallel` não participa do ALNS.
- `makespan` é calculado como máximo atraso e persistido no CSV.
- A saída CSV continua compatível com a rotina de atualização de cabeçalho.

## Validação mínima

### Compilação

Executar compilação completa do projeto após as alterações.

### Execução funcional

Executar ao menos:

1. um cenário com `--destoryStrategy GeneticAlgo`;
2. um cenário com `--destoryStrategy GeneticAlgoParallel --num_threads 2`.

Validar:

- ausência de falha imediata de execução;
- geração de resultado com coluna `makespan`;
- compatibilidade do CSV atualizado;
- seleção explícita correta da estratégia paralela.

### Verificação de corretude básica

Confirmar que:

- `neighbor.agents` continua sendo produzido no GA sequencial e no paralelo;
- `makespan` é não negativo quando existe solução;
- a execução paralela com `num_threads > 1` não introduz comportamento obviamente inconsistente.

## Observações finais

- Se a reconstrução de `SpaceTimeAStar` por avaliação se mostrar muito custosa, isso deve ser tratado em iteração posterior e não nesta entrega.
- Se o link com threads já funcionar implicitamente no ambiente atual, o ajuste em `CMakeLists.txt` pode ser omitido. Caso contrário, ele passa a ser obrigatório.

## Adjustments

### Adjustment 1

- datetime: 2026-07-11 14:03:49 -0300

#### Prompt

```text
no arquivo specs/2-parallel-genetic-algorithm-makespan/spec.md na seção fora do escopo, dois pontos foram mencionados: "Melhorias agressivas de performance além do necessário para obter corretude da execução paralela". e "Mudanças nas demais heurísticas de destruição além do necessário para preservar compatibilidade com ALNS." Explique melhor cada uma dessas afirmações
```

#### Response

```text
Esses dois itens existem para limitar o escopo da implementação e evitar que a feature cresça para uma refatoração ampla do solver. Eles aparecem em spec.md:29 e spec.md:30.

A frase sobre “melhorias agressivas de performance” quer dizer: nesta entrega, o objetivo principal é fazer a versão paralela funcionar corretamente, sem corrida de dados e com resultado coerente. Então ficam fora do escopo mudanças como cache sofisticado de SpaceTimeAStar por thread, pool de threads, reuso complexo de estruturas de busca, redesign da avaliação de fitness para reduzir cópias de PathTable, ou ajustes finos para escalar melhor com populações grandes. Se algo disso for necessário para a corretude mínima, entra. Se for apenas otimização, fica para depois.

A frase sobre “mudanças nas demais heurísticas de destruição” quer dizer: a feature nova deve mexer no que for necessário para introduzir GeneticAlgoParallel sem quebrar o comportamento existente do ALNS, mas não deve redesenhar RandomWalk, Intersection, Random, pesos adaptativos ou a lógica de seleção dessas heurísticas além do indispensável. Em termos práticos, você pode ajustar enumeração, contagem e indexação para garantir que a nova estratégia não entre no ALNS por acidente, mas não deve aproveitar essa tarefa para recalibrar pesos, mudar critérios de seleção ou alterar o comportamento das heurísticas antigas.
```

#### Changes

- before: a seção `Fora de escopo` citava os dois limites de forma curta e abstrata. after: a seção agora explicita exemplos concretos de otimizações excluídas nesta entrega e delimita com mais precisão quais mudanças nas heurísticas antigas continuam fora do escopo.
- before: a explicação dada em conversa não estava preservada no artefato da feature. after: o `spec.md` agora registra o prompt e a resposta em uma seção de ajustes para referência futura.

### Adjustment 2

- datetime: 2026-07-16 20:31:43 -0300

#### Prompt

```text
faça as seguintes alterações na função generateNeighborByGeneticAlgorithm:

* a função evaluate_fitness é muito cara para ser chamada 3 vezes na função generateNeighborByGeneticAlgorithm
* mude o comportamento da função de forma que a função é chamada uma vez a cada loop evolutivo e somente na etapa inicial do algortimo genético: na etapa de avaliação. No último loop, somente a etapa de avaliação é feita
* na etapa de avaliação, guardar sempre o Id do indivíduo mais apto na fitness_scores
* mude o comportamento na parte de crossover: os 4 indivíduos mais aptos sempre geram 4 filhos com os 4 indivíduos menos aptos. Esses 4 filhos sempre substituem os 4 indivíduos menos aptos. Não chamar a função evaluate_fitness
* mude o comportamento da mutação: ela pode acontecer com todos os indivíduos menos no indivíduo mais apto. Não chamar a função evaluate_fitness
* sempre fazer uma iteração a no loop evolutivo, para que na última iteração seja executada somente a fase de avaliação de indivíduos
* após o loop evolutivo, não chamar a função evaluate fitness, fazer apenas neighbor.agents = population[best_idx]

anote essas mudanças em specs/2-parallel-genetic-algorithm-makespan/spec.md
```

#### Changes

- before: o spec descrevia o GA sequencial apenas como correção da passagem explícita de parâmetros no `run()`.
- after: o spec passou a exigir uma única avaliação de fitness por iteração, com iteração final dedicada apenas à avaliação e reutilização de `best_idx` sem reavaliação final.
- before: o spec não fixava o comportamento de crossover e mutação do GA sequencial após a avaliação.
- after: o spec passou a definir crossover entre os 4 melhores e os 4 piores, substituição direta dos piores e mutação em todos os indivíduos exceto o melhor, sem novas chamadas de `evaluateFitness` dentro da geração.

### Adjustment 3

- datetime: 2026-07-11 14:07:24 -0300

#### Prompt

```text
adicione no arquivo de spec os dados de auditoria, definidos em .github/copilot-instructions.md
```

#### Changes

- before: o `spec.md` não tinha uma seção explícita de auditoria no topo do arquivo. after: o arquivo agora começa com a seção `Auditoria`, contendo `created_at`, `author`, `last_updated` e `ai_model`.
- before: esta alteração de metadados não estava registrada no histórico do spec. after: o ajuste passou a constar na seção `Adjustments` com timestamp e prompt integral.