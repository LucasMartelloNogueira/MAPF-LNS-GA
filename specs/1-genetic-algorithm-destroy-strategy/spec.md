# Spec: Genetic Algorithm Destroy Strategy para LNS

## Objetivo

Implementar uma nova estratégia de destruição (`GENETIC_ALGO`) no algoritmo MAPF-LNS que utiliza um algoritmo genético para selecionar o melhor subgrupo de agentes a ser replanejado. Os parâmetros do GA (tamanho da população, número de gerações, taxa de mutação) devem ser configuráveis via CLI.

## Escopo

- Adicionar novo valor `GENETIC_ALGO` ao enum `destroy_heuristic`
- Implementar método `generateNeighborByGeneticAlgorithm` com GA completo
- Implementar métodos auxiliares para geração de população inicial
- Implementar avaliação de fitness via replaneamento PP
- Adicionar parâmetros CLI: `--gaPopSize`, `--gaGenerations`, `--gaMutationRate`
- Integrar com ALNS

## Pré-requisitos

- Projeto compila com `cmake . && make`
- Arquivos existentes: `inc/LNS.h`, `src/LNS.cpp`, `src/driver.cpp`

---

## Passos de Implementação

### Passo 1: Alterar `inc/LNS.h` — Enum

Adicionar `GENETIC_ALGO` antes de `DESTORY_COUNT`:

```cpp
enum destroy_heuristic { RANDOMAGENTS, RANDOMWALK, INTERSECTION, GENETIC_ALGO, DESTORY_COUNT };
```

### Passo 2: Alterar `inc/LNS.h` — Atributos do GA

Adicionar na seção `private` da classe `LNS`, após os atributos de ALNS:

```cpp
// Genetic Algorithm parameters (configurable via CLI)
int ga_population_size = 8;
int ga_num_generations = 3;
double ga_mutation_rate = 0.20;
```

### Passo 3: Alterar `inc/LNS.h` — Declaração de Métodos

Adicionar na seção `private`, após `generateNeighborByIntersection`:

```cpp
// Genetic Algorithm destroy strategy
bool generateNeighborByGeneticAlgorithm(int population_size = -1, int num_generations = -1, double mutation_rate = -1);
vector<int> getAgentsByRandomWalkForGA();
vector<int> getAgentsByIntersectionForGA();
vector<int> getAgentsByRandomForGA(int seed);
int evaluateFitness(const vector<int>& individual);
```

### Passo 4: Alterar `inc/LNS.h` — Assinatura do Construtor

Atualizar a assinatura do construtor para receber parâmetros do GA:

```cpp
LNS(const Instance& instance, double time_limit,
    string init_algo_name, string replan_algo_name, string destory_name,
    int neighbor_size, int num_of_iterations, int screen, PIBTPPS_option pipp_option,
    int ga_pop_size = 8, int ga_gens = 3, double ga_mut_rate = 0.20);
```

### Passo 5: Alterar `src/LNS.cpp` — Construtor (assinatura + parâmetros GA)

Atualizar a assinatura da implementação do construtor adicionando os novos parâmetros:

```cpp
LNS::LNS(const Instance& instance, double time_limit, string init_algo_name, string replan_algo_name, string destory_name,
         int neighbor_size, int num_of_iterations, int screen, PIBTPPS_option pipp_option,
         int ga_pop_size, int ga_gens, double ga_mut_rate) :
         instance(instance), time_limit(time_limit), init_algo_name(std::move(init_algo_name)),
         replan_algo_name(replan_algo_name), neighbor_size(neighbor_size), num_of_iterations(num_of_iterations),
         screen(screen), path_table(instance.map_size), pipp_option(pipp_option), replan_time_limit(time_limit / 100)
```

Dentro do corpo do construtor, após o bloco de `if/else if` do `destory_name`, adicionar:

```cpp
else if (destory_name == "GeneticAlgo")
    destroy_strategy = GENETIC_ALGO;
```

E no final do construtor (antes do `preprocessing_time`), adicionar atribuição dos parâmetros GA:

```cpp
// Set GA parameters from CLI
if (ga_pop_size > 0) ga_population_size = ga_pop_size;
if (ga_gens > 0) ga_num_generations = ga_gens;
if (ga_mut_rate >= 0) ga_mutation_rate = ga_mut_rate;
```

### Passo 6: Alterar `src/LNS.cpp` — Switch Case no `run()`

No switch dentro do método `run()`, antes do `default:`, adicionar:

```cpp
case GENETIC_ALGO:
    succ = generateNeighborByGeneticAlgorithm();
    break;
```

### Passo 7: Implementar `generateNeighborByGeneticAlgorithm` em `src/LNS.cpp`

Adicionar após o método `generateNeighborByRandomWalk()`:

```cpp
bool LNS::generateNeighborByGeneticAlgorithm(int population_size, int num_generations, double mutation_rate) {
    const int POP_SIZE = (population_size > 0) ? population_size : ga_population_size;
    const int NUM_GENERATIONS = (num_generations > 0) ? num_generations : ga_num_generations;
    const double MUTATION_RATE = (mutation_rate >= 0) ? mutation_rate : ga_mutation_rate;

    // 1. Gerar população inicial (POP_SIZE indivíduos)
    vector<vector<int>> population;

    // 1 indivíduo via random walk adaptado
    population.push_back(getAgentsByRandomWalkForGA());

    // 1 indivíduo via intersection adaptado
    population.push_back(getAgentsByIntersectionForGA());

    // Restante: indivíduos aleatórios com seeds diferentes
    for (int seed = 0; seed < POP_SIZE - 2; seed++) {
        population.push_back(getAgentsByRandomForGA(seed));
    }

    // 2. Loop evolutivo
    for (int gen = 0; gen < NUM_GENERATIONS; gen++) {
        // 2a. Avaliar fitness de cada indivíduo
        vector<pair<int, int>> fitness_scores; // (fitness, index)
        for (int i = 0; i < (int)population.size(); i++) {
            int fitness = evaluateFitness(population[i]);
            fitness_scores.push_back({fitness, i});
        }

        // 2b. Ordenar por fitness (menor sum_of_costs = melhor)
        sort(fitness_scores.begin(), fitness_scores.end());

        // 2c. Seleção para crossover: usar os 4 mais aptos como pais
        vector<vector<int>> parents;
        for (int i = 0; i < 4 && i < (int)fitness_scores.size(); i++) {
            parents.push_back(population[fitness_scores[i].second]);
        }

        // 2d. Crossover por ponto de corte para gerar 4 novos filhos
        vector<vector<int>> children;
        for (int i = 0; i + 1 < (int)parents.size(); i += 2) {
            auto& parent1 = parents[i];
            auto& parent2 = parents[i + 1];

            int cut_point = rand() % min(parent1.size(), parent2.size());

            vector<int> child1, child2;
            set<int> child1_set, child2_set;

            // Primeira parte do parent1 para child1
            for (int j = 0; j <= cut_point && j < (int)parent1.size(); j++) {
                child1.push_back(parent1[j]);
                child1_set.insert(parent1[j]);
            }
            // Complementar com parent2 (sem duplicados)
            for (int j = 0; j < (int)parent2.size() && (int)child1.size() < neighbor_size; j++) {
                if (child1_set.find(parent2[j]) == child1_set.end()) {
                    child1.push_back(parent2[j]);
                    child1_set.insert(parent2[j]);
                }
            }

            // Primeira parte do parent2 para child2
            for (int j = 0; j <= cut_point && j < (int)parent2.size(); j++) {
                child2.push_back(parent2[j]);
                child2_set.insert(parent2[j]);
            }
            // Complementar com parent1 (sem duplicados)
            for (int j = 0; j < (int)parent1.size() && (int)child2.size() < neighbor_size; j++) {
                if (child2_set.find(parent1[j]) == child2_set.end()) {
                    child2.push_back(parent1[j]);
                    child2_set.insert(parent1[j]);
                }
            }

            children.push_back(child1);
            children.push_back(child2);
        }

        // 2e. Mutação nos filhos
        for (int i = 0; i < (int)children.size(); i++) {
            for (int j = 0; j < (int)children[i].size(); j++) {
                if ((double)rand() / RAND_MAX < MUTATION_RATE) {
                    int new_agent = rand() % agents.size();
                    set<int> current_set(children[i].begin(), children[i].end());
                    int attempts = 0;
                    while (current_set.count(new_agent) > 0 && attempts < 20) {
                        new_agent = rand() % agents.size();
                        attempts++;
                    }
                    if (current_set.count(new_agent) == 0) {
                        children[i][j] = new_agent;
                    }
                }
            }
        }

        // 2f. Manter população constante: adicionar filhos e remover os piores
        for (auto& child : children) {
            population.push_back(child);
        }
        // Reavaliar fitness de toda a população
        vector<pair<int, int>> full_fitness;
        for (int i = 0; i < (int)population.size(); i++) {
            int f = evaluateFitness(population[i]);
            full_fitness.push_back({f, i});
        }
        sort(full_fitness.begin(), full_fitness.end());
        // Manter apenas os POP_SIZE melhores
        vector<vector<int>> surviving_population;
        for (int i = 0; i < POP_SIZE && i < (int)full_fitness.size(); i++) {
            surviving_population.push_back(population[full_fitness[i].second]);
        }
        population = surviving_population;
    }

    // 3. Selecionar o melhor indivíduo da população final
    int best_fitness = INT_MAX;
    int best_idx = 0;
    for (int i = 0; i < (int)population.size(); i++) {
        int fitness = evaluateFitness(population[i]);
        if (fitness < best_fitness) {
            best_fitness = fitness;
            best_idx = i;
        }
    }

    neighbor.agents = population[best_idx];
    if (neighbor.agents.empty())
        return false;

    if (screen >= 2)
        cout << "Generate " << neighbor.agents.size() << " neighbors by genetic algorithm" << endl;
    return true;
}
```

### Passo 8: Implementar `getAgentsByRandomWalkForGA` em `src/LNS.cpp`

Adicionar após `generateNeighborByGeneticAlgorithm`:

```cpp
vector<int> LNS::getAgentsByRandomWalkForGA() {
    if (neighbor_size >= (int)agents.size()) {
        vector<int> all_agents(agents.size());
        for (int i = 0; i < (int)agents.size(); i++)
            all_agents[i] = i;
        return all_agents;
    }

    // Save tabu_list to avoid affecting the main LNS loop state
    auto saved_tabu_list = tabu_list;

    int a = findMostDelayedAgent();
    if (a < 0) {
        tabu_list = saved_tabu_list; // restore
        return getAgentsByRandomForGA(42);
    }

    set<int> neighbors_set;
    neighbors_set.insert(a);
    randomWalk(a, agents[a].path[0].location, 0, neighbors_set, neighbor_size, (int)agents[a].path.size() - 1);
    int count = 0;
    while ((int)neighbors_set.size() < neighbor_size && count < 10) {
        int t = rand() % agents[a].path.size();
        randomWalk(a, agents[a].path[t].location, t, neighbors_set, neighbor_size, (int)agents[a].path.size() - 1);
        count++;
        int idx = rand() % neighbors_set.size();
        int i = 0;
        for (auto n : neighbors_set) {
            if (i == idx) { a = n; break; }
            i++;
        }
    }

    // Restore tabu_list so GA doesn't affect main LNS loop
    tabu_list = saved_tabu_list;

    return vector<int>(neighbors_set.begin(), neighbors_set.end());
}
```

### Passo 9: Implementar `getAgentsByIntersectionForGA` em `src/LNS.cpp`

```cpp
vector<int> LNS::getAgentsByIntersectionForGA() {
    if (intersections.empty()) {
        for (int i = 0; i < instance.map_size; i++) {
            if (!instance.isObstacle(i) && instance.getDegree(i) > 2)
                intersections.push_back(i);
        }
    }

    set<int> neighbors_set;
    auto pt = intersections.begin();
    std::advance(pt, rand() % intersections.size());
    int location = *pt;
    path_table.get_agents(neighbors_set, neighbor_size, location);

    if ((int)neighbors_set.size() < neighbor_size) {
        set<int> closed;
        closed.insert(location);
        std::queue<int> open;
        open.push(location);
        while (!open.empty() && (int)neighbors_set.size() < neighbor_size) {
            int curr = open.front();
            open.pop();
            for (auto next : instance.getNeighbors(curr)) {
                if (closed.count(next) > 0) continue;
                open.push(next);
                closed.insert(next);
                if (instance.getDegree(next) >= 3) {
                    path_table.get_agents(neighbors_set, neighbor_size, next);
                    if ((int)neighbors_set.size() == neighbor_size) break;
                }
            }
        }
    }

    vector<int> result(neighbors_set.begin(), neighbors_set.end());
    if ((int)result.size() > neighbor_size) {
        std::random_shuffle(result.begin(), result.end());
        result.resize(neighbor_size);
    }
    return result;
}
```

### Passo 10: Implementar `getAgentsByRandomForGA` em `src/LNS.cpp`

```cpp
vector<int> LNS::getAgentsByRandomForGA(int seed) {
    std::mt19937 rng(seed);
    vector<int> all_agents(agents.size());
    for (int i = 0; i < (int)agents.size(); i++)
        all_agents[i] = i;
    std::shuffle(all_agents.begin(), all_agents.end(), rng);
    all_agents.resize(min((int)all_agents.size(), neighbor_size));
    return all_agents;
}
```

### Passo 11: Implementar `evaluateFitness` em `src/LNS.cpp`

```cpp
int LNS::evaluateFitness(const vector<int>& individual) {
    int current_sum_of_costs = sum_of_costs;

    // Create local copy of path_table to avoid affecting global state
    PathTable local_path_table = path_table;

    // Save original paths and calculate old cost
    vector<Path> original_paths(individual.size());
    int old_individual_cost = 0;
    for (int i = 0; i < (int)individual.size(); i++) {
        original_paths[i] = agents[individual[i]].path;
        old_individual_cost += (int)agents[individual[i]].path.size() - 1;
    }

    // Remove paths from local path_table
    for (int i = 0; i < (int)individual.size(); i++) {
        local_path_table.deletePath(individual[i], original_paths[i]);
    }

    // Replan using PP on local_path_table (does not modify global agents or path_table)
    int new_individual_cost = 0;
    bool success = true;
    vector<Path> new_paths(individual.size());

    for (int i = 0; i < (int)individual.size(); i++) {
        int id = individual[i];
        Path new_path = agents[id].path_planner.findOptimalPath(local_path_table);
        if (new_path.empty()) {
            success = false;
            break;
        }
        new_paths[i] = new_path;
        new_individual_cost += (int)new_path.size() - 1;
        local_path_table.insertPath(id, new_path);
    }

    // Calculate fitness
    int fitness;
    if (success) {
        fitness = current_sum_of_costs - old_individual_cost + new_individual_cost;
    } else {
        fitness = INT_MAX;
    }

    // No restore needed — global agents and path_table were never modified
    return fitness;
}
```

### Passo 12: Alterar `src/LNS.cpp` — Integrar com ALNS

No método `chooseDestroyHeuristicbyALNS()`, no switch, adicionar:

```cpp
case 3 : destroy_strategy = GENETIC_ALGO; break;
```

### Passo 13: Alterar `src/driver.cpp` — Adicionar opções CLI

Na seção de opções do `po::options_description`, após `destoryStrategy`, adicionar:

```cpp
("gaPopSize", po::value<int>()->default_value(8),
        "Population size for Genetic Algorithm destroy strategy")
("gaGenerations", po::value<int>()->default_value(3),
        "Number of generations for Genetic Algorithm destroy strategy")
("gaMutationRate", po::value<double>()->default_value(0.20),
        "Mutation rate for Genetic Algorithm destroy strategy (0.0 to 1.0)")
```

Atualizar a descrição do `destoryStrategy` para incluir `GeneticAlgo`:

```cpp
("destoryStrategy", po::value<string>()->default_value("Adaptive"),
        "Heuristics for finding subgroups (Random, RandomWalk, Intersection, GeneticAlgo, Adaptive)")
```

### Passo 14: Alterar `src/driver.cpp` — Passar parâmetros GA ao LNS

Na seção onde o objeto `LNS` é instanciado, ler os parâmetros e passá-los:

```cpp
int ga_pop_size = vm["gaPopSize"].as<int>();
int ga_generations = vm["gaGenerations"].as<int>();
double ga_mutation_rate = vm["gaMutationRate"].as<double>();
```

Atualizar a chamada do construtor `LNS` para incluir os novos parâmetros.

### Passo 15: Adicionar `#include <random>` em `src/LNS.cpp`

Necessário para `std::mt19937` e `std::shuffle` usados em `getAgentsByRandomForGA`.

```cpp
#include <random>
```

---

## Validação

1. **Compilação**: `cmake . && make` sem erros
2. **Teste básico**: 
   ```bash
   ./lns -m random-32-32-20.map -a random-32-32-20-random-1.scen -k 50 -t 60 --destoryStrategy GeneticAlgo
   ```
3. **Teste com parâmetros CLI**:
   ```bash
   ./lns -m random-32-32-20.map -a random-32-32-20-random-1.scen -k 50 -t 60 --destoryStrategy GeneticAlgo --gaPopSize 10 --gaGenerations 5 --gaMutationRate 0.30
   ```
4. **Verificar** que a saída mostra iterações normais com soluções válidas (sem crash/assertion errors)
5. **Comparar** com estratégia `RandomWalk` para verificar que o custo final é razoável

---

## Notas

- O `evaluateFitness` é chamado múltiplas vezes por geração. Se o desempenho for insuficiente, considerar caching de resultados de fitness para indivíduos não alterados entre gerações.
- A população se mantém constante em `POP_SIZE`: filhos entram e os piores saem.
- O `findMostDelayedAgent()` usado em `getAgentsByRandomWalkForGA` modifica a `tabu_list`. Isso é intencional — mantém diversidade entre chamadas.

### Isolamento de Estado entre Soluções

Para garantir que a avaliação de cada indivíduo do GA seja independente e não corrompa o estado global nem o de outras soluções, as seguintes variáveis são copiadas localmente:

| Variável | Função | Motivo da Cópia |
|----------|--------|----------------|
| `path_table` | `evaluateFitness` | O replaneamento PP executa `deletePath` e `insertPath`. Usar a tabela global causaria interferência entre avaliações de indivíduos diferentes, pois o estado de um afetaria a busca de caminho do próximo. |
| `agents[].path` | `evaluateFitness` | O replaneamento sobrescreve `agents[id].path`. Sem isolamento, um indivíduo avaliado modificaria os caminhos reais dos agentes, afetando o cálculo de fitness de todos os demais. |
| `tabu_list` | `getAgentsByRandomWalkForGA` | `findMostDelayedAgent()` insere IDs na `tabu_list` do objeto LNS. Sem save/restore, a geração de população do GA modificaria permanentemente a tabu_list, afetando iterações futuras do loop LNS principal. |

**Implementação:**
- `evaluateFitness`: usa `PathTable local_path_table = path_table` (cópia completa). Novos caminhos são armazenados em `vector<Path> new_paths` local, nunca em `agents[id].path`.
- `getAgentsByRandomWalkForGA`: faz `auto saved_tabu_list = tabu_list` no início e `tabu_list = saved_tabu_list` ao final.
