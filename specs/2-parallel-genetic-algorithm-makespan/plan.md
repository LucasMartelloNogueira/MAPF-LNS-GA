# Plano: Paralelização do Genetic Algorithm e cálculo de makespan no MAPF-LNS

## Auditoria

- created_at: 2026-07-11 12:47:14 -0300
- author: Lucas Martello Nogueira
- last_updated: 2026-07-11 13:40:32 -0300

## Prompt

```text
Leia o arquivo .github/copilot-instructions.md e faça um plano para a seguintes funcionalidades

funcionalidades: criar uma implementação para resolver o problema do multi agent path finding (MAPF) partindo da abordagem de algoritmo genético mas agora paralelizar o cálculo das funções objetivos da população em diferentes threads. Além disso calcular uma nova função objetivo do problema em geral que é o makespan: o máximo do atraso de todos os agentes.

para fazer a funcionalidade, faça os seguintes ajustes

* em src/driver.cpp, na função main, adicione uma nova opção na cli chamada "num_threads", ela deve ser o número de threads usada na solução paralela. O valor default é 1
* em src/driver.cpp, na função main, adicione um novo valor na opção "destoryStrategy" chamado "GeneticAlgoParallel"
* adicione em inc/LNS.h na classe LNS uma nova variavel chamada makespan
* em src/LNS.cpp, na classe LNS, crie um novo método, usando o método generateNeighborByGeneticAlgorithm como referencia. para criar um novo método que faz a mesma coisa mas que avalie o fitness de cada indivíduo da população de forma paralela com threads. O número de threads usado deve ser o mesmo descrito pela opção "num_threads" na cli. O nome do novo método deve ser "generateNeighborByGeneticAlgorithmParallel". Esse método deve ter o mesmos parametros que o método generateNeighborByGeneticAlgorithm e um novo parâmetro chamado num_threads, que define o número de threads usados no algoritmo. O retorno do método generateNeighborByGeneticAlgorithmParallel deve ser o mesmo que o método generateNeighborByGeneticAlgorithm
* em src/LNS.cpp, na classe LNS, adicione no método construtor um novo caso que considera a opção GeneticAlgoParallel para setar o valor da variável destroy_strategy sendo GENETIC_ALGO_PARALLEL. 
* em src/LNS.cpp, na classe LNS, adicione no método run um novo caso no switch de destroy_strategy, que considera o caso em que destroy_strategy == GENETIC_ALGO_PARALLEL. Caso entre nesse caso, use o algortimo generateNeighborByGeneticAlgorithmParallel
* em src/LNS.cpp, na classe LNS, no método run, adicione o cálculo do makespan
* em src/LNS.cpp, na classe LNS, no método writeResultToFile, adicione o header makespan e seu valor nos dados
* sem src/LNS.cpp, o método generateNeighborByGeneticAlgorithm não é chamado com os argumentos necessários no switch case do destroy_strategy. Passe os argumentos necessários explicitamente para esse método e nas variáveis POP_SIZE, NUM_GENERATIONS e MUTATION_RATE, pegue seus valores dos argumentos passados. Use o mesmo mecanismo de passagem dos argumentos para o método generateNeighborByGeneticAlgorithmParallel
```

## 1. Contexto do codebase

### 1.1 Contexto do problema e da solução

- O repositório implementa MAPF em tempo discreto sobre grafos, com foco em soluções livres de conflito para múltiplos agentes.
- A solução principal segue a arquitetura MAPF-LNS: gera uma solução inicial e depois itera entre destroy e repair para reduzir o custo total.
- O custo principal atual do LNS é `sum_of_costs`, enquanto `sum_of_distances` é usado como estatística auxiliar e base para o cálculo de AUC.

### 1.2 Estado atual do código

- O enum `destroy_heuristic` em `inc/LNS.h` já contém `GENETIC_ALGO`.
- O construtor de `LNS` em `src/LNS.cpp` já reconhece `destory_name == "GeneticAlgo"`.
- A CLI em `src/driver.cpp` já expõe `gaPopSize`, `gaGenerations` e `gaMutationRate`.
- O método `generateNeighborByGeneticAlgorithm` já existe em `src/LNS.cpp` e usa `evaluateFitness` para escolher o melhor indivíduo.
- O método `writeResultToFile` já grava colunas específicas do algoritmo genético.

### 1.3 Lacunas entre o estado atual e a funcionalidade pedida

- Ainda não existe a opção de CLI `num_threads`.
- Ainda não existe o valor `GeneticAlgoParallel` no fluxo de escolha de heurística de destruição.
- A classe `LNS` ainda não armazena `makespan` como estatística da solução.
- O `switch` de `run()` chama `generateNeighborByGeneticAlgorithm()` sem passar explicitamente `ga_population_size`, `ga_num_generations` e `ga_mutation_rate`.
- Ainda não existe o método `generateNeighborByGeneticAlgorithmParallel`.
- O arquivo de resultados não contém a coluna `makespan`.

### 1.4 Riscos técnicos relevantes para paralelização

- O fitness atual chama `agents[id].path_planner.findOptimalPath(local_path_table)`. Esse solver mantém estruturas internas mutáveis, portanto duas threads avaliando indivíduos que compartilham algum agente podem causar corrida de dados.
- O desempate do A* usa `rand()` em comparadores definidos em `inc/SingleAgentSolver.h`. Em execução multi-thread isso introduz acesso concorrente ao RNG global.
- O plano precisa tratar esses dois pontos para que a paralelização seja correta de fato, e não apenas sintaticamente paralela.

## 2. Bibliotecas e documentação

### 2.1 Bibliotecas já disponíveis e reutilizáveis

- STL: `vector`, `set`, `queue`, `algorithm`, `random`.
- Infraestrutura atual do projeto: `SpaceTimeAStar`, `PathTable`, `Agent`, `Neighbor`.
- Boost continua sendo usado apenas para CLI e não precisa ser estendido para a paralelização.

### 2.2 Bibliotecas a usar

- `std::thread` para distribuir a avaliação de fitness em lotes.
- `std::mutex` apenas se algum ponto residual precisar de serialização controlada.
- `std::atomic<int>` ou escrita por índice em vetores pré-alocados para coletar resultados sem contenção desnecessária.
- `Threads::Threads` no CMake, se necessário, para garantir o link correto com suporte a threads no toolchain atual.

### 2.3 Justificativa técnica

- A paralelização requerida é local ao cálculo de fitness da população e não exige dependência externa nem runtime de task scheduling mais complexo.
- O uso de threads explícitas preserva compatibilidade com o restante da base C++14.
- Como o código já possui uma implementação sequencial do GA, a evolução natural é compartilhar o fluxo evolutivo e substituir apenas a etapa de avaliação por uma versão paralela e segura.
- A prioridade desta entrega é corretude da abordagem paralela. Ganhos de escalabilidade ficam em segundo plano e só devem ser perseguidos depois que a execução paralela estiver isolada de corridas de dados e reproduzível o suficiente para validação.

## 3. Arquivos que precisam ser alterados ou criados

| Arquivo | Tipo de mudança | Objetivo |
|---|---|---|
| `src/driver.cpp` | alterar | adicionar `num_threads` e expor `GeneticAlgoParallel` na CLI |
| `inc/LNS.h` | alterar | incluir `GENETIC_ALGO_PARALLEL`, `makespan`, `num_threads` e novas assinaturas |
| `src/LNS.cpp` | alterar | integrar a nova estratégia, calcular `makespan`, passar parâmetros explicitamente e implementar avaliação paralela |
| `inc/SingleAgentSolver.h` | alterar | trocar o uso inseguro de `rand()` por uma função de desempate pseudoaleatória seeded com o id da thread |
| `CMakeLists.txt` | alterar, se necessário | garantir link correto com threads |
| `docs/parallel-ga-makespan.md` | criar na implementação | documentar a nova estratégia, parâmetro `num_threads` e definição adotada de makespan |

## 4. Estratégia de implementação

### 4.1 CLI e configuração do solver

Adicionar uma nova opção na CLI:

```cpp
("num_threads", po::value<int>()->default_value(1),
    "number of threads used by the parallel genetic algorithm")
```

Atualizar o construtor de `LNS` para receber `num_threads` e persisti-lo em um atributo próprio:

```cpp
LNS(const Instance& instance, double time_limit,
    string init_algo_name, string replan_algo_name, string destory_name,
    int neighbor_size, int num_of_iterations, int screen, PIBTPPS_option pipp_option,
    int ga_pop_size = 8, int ga_gens = 3, double ga_mut_rate = 0.20,
    int num_threads = 1);
```

No construtor de `src/LNS.cpp`, adicionar o novo caso:

```cpp
else if (destory_name == "GeneticAlgoParallel")
    destroy_strategy = GENETIC_ALGO_PARALLEL;
```

### 4.2 Extensão do enum e do estado do LNS

Atualizar o enum em `inc/LNS.h`:

```cpp
enum destroy_heuristic {
    RANDOMAGENTS,
    RANDOMWALK,
    INTERSECTION,
    GENETIC_ALGO,
    GENETIC_ALGO_PARALLEL,
    DESTORY_COUNT
};
```

Adicionar os novos atributos:

```cpp
int makespan = -1;
int num_threads = 1;
```

Declarar o novo método:

```cpp
bool generateNeighborByGeneticAlgorithmParallel(
    int population_size = -1,
    int num_generations = -1,
    double mutation_rate = -1,
    int num_threads = -1);
```

### 4.3 Correção do fluxo atual do GA sequencial

O `switch` em `run()` deve passar os argumentos explicitamente para o método já existente, em vez de depender apenas de defaults internos:

```cpp
case GENETIC_ALGO:
    succ = generateNeighborByGeneticAlgorithm(
        ga_population_size,
        ga_num_generations,
        ga_mutation_rate);
    break;
```

O mesmo padrão deve ser aplicado ao novo caso paralelo:

```cpp
case GENETIC_ALGO_PARALLEL:
    succ = generateNeighborByGeneticAlgorithmParallel(
        ga_population_size,
        ga_num_generations,
        ga_mutation_rate,
        num_threads);
    break;
```

Mesmo mantendo os defaults no método, o plano recomenda a passagem explícita para eliminar ambiguidade e deixar o comportamento do `run()` auditável pela CLI.

### 4.4 Implementação do GA paralelo

O novo método `generateNeighborByGeneticAlgorithmParallel` deve reaproveitar o pipeline do GA sequencial:

1. gerar população inicial;
2. avaliar fitness da população;
3. selecionar pais;
4. executar crossover;
5. executar mutação;
6. reavaliar população expandida;
7. manter os melhores indivíduos;
8. selecionar o melhor indivíduo final para `neighbor.agents`.

O ponto de mudança é apenas a avaliação do fitness em lote.

Exemplo de esqueleto:

```cpp
vector<int> fitness(population.size(), INT_MAX);
int worker_count = std::max(1, std::min(requested_threads, (int)population.size()));
vector<std::thread> workers;

auto evaluate_range = [&](int begin, int end)
{
    for (int idx = begin; idx < end; idx++)
        fitness[idx] = evaluateFitnessThreadSafe(population[idx]);
};

for (int worker = 0; worker < worker_count; worker++)
{
    int begin = worker * population.size() / worker_count;
    int end = (worker + 1) * population.size() / worker_count;
    workers.emplace_back(evaluate_range, begin, end);
}

for (auto& worker : workers)
    worker.join();
```

### 4.5 Isolamento thread-safe da avaliação de fitness

Este é o ponto mais importante do plano. Para a avaliação paralela funcionar corretamente, cada thread precisa operar sem compartilhar estado mutável do solver.

Abordagem recomendada:

1. manter `PathTable local_path_table = path_table` por avaliação, como já existe;
2. não reutilizar `agents[id].path_planner` diretamente na avaliação paralela;
3. criar um solver local por agente dentro da avaliação:

```cpp
SpaceTimeAStar local_solver(instance, id);
Path new_path = local_solver.findOptimalPath(local_path_table);
```

Isso evita corridas sobre `open_list`, `focal_list` e `allNodes_table` do `SpaceTimeAStar` armazenado em cada `Agent`.

Se o custo de reconstruir heurísticas por avaliação se mostrar alto, a implementação posterior pode evoluir para um cache por thread. O plano, porém, prioriza corretude primeiro.

### 4.6 Ajuste do RNG para execução paralela

O desempate aleatório em `inc/SingleAgentSolver.h` usa `rand()`, o que não é seguro em multi-thread.

O ajuste solicitado para o plano é usar o id da thread como seed para uma função randômica usada no desempate.

Abordagem recomendada:

1. criar uma função auxiliar em `inc/SingleAgentSolver.h` ou em ponto equivalente acessível ao solver;
2. obter `std::this_thread::get_id()`;
3. transformar o id em seed numérica com `std::hash<std::thread::id>`;
4. combinar essa seed com alguns campos do nó para evitar repetir sempre o mesmo resultado dentro da mesma thread;
5. usar o valor gerado apenas para desempate, sem depender de `rand()` global.

Exemplo de direção para a implementação:

```cpp
inline bool threadSeededTieBreak(int lhs, int rhs, int timestep)
{
    auto tid_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    std::mt19937 rng(static_cast<unsigned int>(tid_hash ^ (lhs * 73856093u) ^ (rhs * 19349663u) ^ (timestep * 83492791u)));
    return (rng() & 1u) == 0u;
}
```

Essa solução não é pensada para máxima performance. Ela atende primeiro ao requisito de corretude e de isolamento entre threads.

### 4.7 Cálculo de makespan

O pedido do usuário define makespan como o máximo do atraso entre todos os agentes. Nesta base de código, atraso já é expresso por:

```cpp
agent.getNumOfDelays()
```

Portanto, no `run()` o cálculo deve ser algo como:

```cpp
makespan = 0;
for (const auto& agent : agents)
    makespan = std::max(makespan, agent.getNumOfDelays());
```

Esse cálculo deve ocorrer:

- após a solução inicial ser obtida;
- e ao fim de cada iteração em que a solução corrente possa ter mudado.

### 4.8 Persistência dos resultados

O cabeçalho de `writeResultToFile` deve incluir `makespan` em posição estável, por exemplo após `solution cost`:

```cpp
"runtime,solution cost,makespan,initial solution cost,..."
```

E a linha de dados deve gravar o valor correspondente:

```cpp
stats << runtime << "," << sum_of_costs << "," << makespan << "," << initial_sum_of_costs << "," ...;
```

Como esse método já possui lógica de migração de cabeçalho, a inclusão da nova coluna deve continuar compatível com arquivos CSV existentes.

## 5. Passo a passo de implementação

1. Atualizar `src/driver.cpp` com `num_threads` e com a descrição da nova estratégia `GeneticAlgoParallel`.
2. Atualizar `inc/LNS.h` com o novo enum, atributos `makespan` e `num_threads`, e a assinatura do método paralelo.
3. Atualizar o construtor em `src/LNS.cpp` para reconhecer `GeneticAlgoParallel` e armazenar `num_threads`.
4. Corrigir o `switch` de `run()` para passar explicitamente `ga_population_size`, `ga_num_generations` e `ga_mutation_rate` ao GA sequencial.
5. Adicionar o novo caso `GENETIC_ALGO_PARALLEL` no `switch` de `run()`.
6. Extrair ou reaproveitar a parte comum de avaliação da população para reduzir duplicação entre GA sequencial e paralelo.
7. Implementar `generateNeighborByGeneticAlgorithmParallel` com avaliação em lotes distribuída por threads.
8. Tornar a avaliação de fitness segura para multi-thread, usando solver local por avaliação e eliminando o uso inseguro de `rand()` na busca de baixo nível.
9. Calcular `makespan` após a solução inicial e após atualizações de solução durante o loop principal.
10. Atualizar `writeResultToFile` para incluir `makespan` e, se desejado, gravar também `num_threads` quando a estratégia paralela estiver ativa.
11. Manter `GeneticAlgoParallel` fora do ALNS, revisando `DESTORY_COUNT`, `destroy_weights` e `chooseDestroyHeuristicbyALNS()` para que a nova estratégia não seja sorteada adaptativamente.
12. Ajustar `CMakeLists.txt` se o ambiente exigir `Threads::Threads` no link.
13. Validar compilação e executar ao menos um cenário com `GeneticAlgo` e outro com `GeneticAlgoParallel` para comparar corretude e formato do CSV.

## 6. Critérios de aceite

- A CLI aceita `--num_threads` e usa `1` por padrão.
- A CLI aceita `--destoryStrategy GeneticAlgoParallel`.
- O enum e o construtor do `LNS` reconhecem `GENETIC_ALGO_PARALLEL`.
- `run()` chama explicitamente o GA sequencial com `ga_population_size`, `ga_num_generations` e `ga_mutation_rate`.
- `run()` chama o GA paralelo com os mesmos parâmetros mais `num_threads`.
- `generateNeighborByGeneticAlgorithmParallel` produz `neighbor.agents` válido e mantém o mesmo contrato de retorno do método sequencial.
- `GeneticAlgoParallel` fica disponível apenas por seleção explícita e não participa do ALNS.
- `makespan` é calculado conforme a definição solicitada: máximo atraso entre todos os agentes.
- O CSV de saída contém a coluna `makespan` e grava seu valor corretamente.
- A execução com `num_threads > 1` compila e roda sem data race observável nas partes novas.

## 7. Riscos, trade-offs e validações

### 7.1 Custo de reconstrução do solver

- Reinstanciar `SpaceTimeAStar` por avaliação aumenta custo fixo de CPU.
- Esse custo é aceitável na primeira versão se for o preço para garantir correção da paralelização.

### 7.2 Balanceamento de carga

- Populações pequenas podem não se beneficiar de muitos threads.
- O método paralelo deve limitar o número efetivo de workers a `min(num_threads, population.size())`.

### 7.3 Semântica de makespan

- Em literatura de MAPF, makespan normalmente significa maior tempo de chegada.
- Neste plano, será seguida estritamente a definição pedida pelo usuário: maior atraso, isto é, `max(agent.getNumOfDelays())`.

### 7.4 Compatibilidade com ALNS

- O `destroy_weights` depende de `DESTORY_COUNT`.
- `GENETIC_ALGO_PARALLEL` deve ficar fora do ALNS.
- Isso significa que `chooseDestroyHeuristicbyALNS()` e qualquer indexação por `DESTORY_COUNT` precisam continuar sorteando apenas as heurísticas já adaptativas.
- A estratégia paralela deve ser ativada apenas quando o usuário selecionar explicitamente `--destoryStrategy GeneticAlgoParallel`.

## 8. Snippets de referência

### 8.1 Chamada explícita do GA no `run()`

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

### 8.2 Atualização do construtor a partir da CLI

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

### 8.3 Cálculo de makespan

```cpp
void LNS::updateMakespan()
{
    makespan = 0;
    for (const auto& agent : agents)
        makespan = std::max(makespan, agent.getNumOfDelays());
}
```

## 9. Sugestões para documentação futura

- Descrever em `docs/parallel-ga-makespan.md` a diferença entre `sum_of_costs` e `makespan` adotado no projeto.
- Registrar exemplos de execução com `GeneticAlgoParallel` e diferentes valores de `num_threads`.
- Documentar que a primeira versão paralela prioriza corretude e pode não escalar linearmente em populações pequenas.

## 10. TODOs identificados

- TODO: centralizar o cálculo de métricas globais da solução (`sum_of_costs`, `sum_of_distances`, `makespan`) em helpers dedicados para reduzir duplicação no `run()`.
- TODO: substituir o uso global de `rand()` em outros pontos do projeto por geradores locais, já que a paralelização tende a ampliar esse risco para além do GA.

## Adjusments

### Adjustment 1

- datetime: 2026-07-11 13:40:32 -0300

#### Prompt

```text
Leia o arquivo specs/2-parallel-genetic-algorithm-makespan/plan.md e faça os seguintes ajustes no plano

* Ajuste do RNG para execução paralela: em inc/SingleAgentSolver.h, pegue o id da thread e use ele como seed para alguma função randômica que retorne um número para desempate
* mantenha o GeneticAlgoParallel fora do ALNS
* foco primeiro na corretude da abordagem paralela antes da escalabilidade
```

#### Changes

- before: a seção de RNG priorizava substituir o desempate por um critério determinístico ou, alternativamente, usar RNG thread-local. after: o plano agora exige explicitamente uma função pseudoaleatória seeded com o id da thread para o desempate em `inc/SingleAgentSolver.h`.
- before: o plano recomendava manter `GeneticAlgoParallel` fora do ALNS, mas ainda como recomendação. after: o plano passou a tratar isso como requisito explícito de implementação e critério de aceite.
- before: a corretude antes da escalabilidade aparecia apenas como observação local no isolamento do solver. after: o plano agora declara essa prioridade como diretriz central de projeto e validação.
