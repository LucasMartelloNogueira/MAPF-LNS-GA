# Plano: Genetic Algorithm Destroy Strategy para LNS

## 1. Análise do Codebase Existente

### Arquitetura Atual

O projeto implementa o algoritmo **MAPF-LNS** (Multi-Agent Path Finding - Large Neighborhood Search). A arquitetura central consiste em:

- **`LNS` (classe principal)**: Controla o loop de otimização. A cada iteração, seleciona um subgrupo de agentes (neighbor), destrói seus caminhos e replaneja usando um algoritmo MAPF.
- **`Neighbor` (struct)**: Armazena a lista de agentes selecionados, seus caminhos antigos e custos.
- **`Agent` (struct)**: Representa um agente com ID, path planner (SpaceTimeAStar) e caminho atual.
- **`PathTable`**: Tabela espaço-temporal que armazena os caminhos de todos os agentes.

### Estratégias de Destruição Existentes

O enum `destroy_heuristic` define as estratégias disponíveis:
```cpp
enum destroy_heuristic { RANDOMAGENTS, RANDOMWALK, INTERSECTION, DESTORY_COUNT };
```

Cada estratégia seleciona um subgrupo de agentes (`neighbor.agents`) para replanejar:

1. **RANDOMAGENTS**: Seleciona `neighbor_size` agentes aleatoriamente (shuffle + resize).
2. **RANDOMWALK**: Começa pelo agente mais atrasado, faz random walk no espaço-tempo e coleta agentes conflitantes.
3. **INTERSECTION**: Seleciona agentes que passam por interseções (vértices com grau > 2) no mapa.

### Fluxo de Replaneamento (runPP)

Após selecionar os agentes:
1. Salva os caminhos antigos e `old_sum_of_costs`.
2. Remove os caminhos dos agentes selecionados da `path_table`.
3. Executa o algoritmo de replaneamento (PP - Prioritized Planning):
   - Para cada agente na ordem, encontra caminho ótimo respeitando restrições da `path_table`.
   - Se o novo `sum_of_costs` é menor que o antigo, aceita os novos caminhos.
   - Caso contrário, restaura os caminhos antigos.

### Padrões e Bibliotecas em Uso

- **C++17** com STL (vector, set, queue, list, etc.)
- **Boost**: program_options, heap, unordered containers
- **Chrono** para controle de tempo
- Sem dependência de bibliotecas externas de metaheurísticas

---

## 2. Bibliotecas e Dependências

Nenhuma biblioteca adicional é necessária. O algoritmo genético será implementado nativamente em C++ usando:
- `<algorithm>` (std::sort, std::random_shuffle)
- `<random>` (std::mt19937 para seeds diferentes na geração de soluções aleatórias)
- `<vector>` (estruturas de dados para população)

**Justificativa**: O algoritmo genético é simples o suficiente para não justificar uma dependência externa, e manter a consistência com o restante do código que implementa tudo de forma nativa.

---

## 3. Plano de Implementação

### 3.1 Arquivos a Modificar

| Arquivo | Alteração |
|---------|-----------|
| `inc/LNS.h` | Adicionar `GENETIC_ALGO` ao enum, declarar novos métodos, adicionar atributos para parâmetros do GA |
| `src/LNS.cpp` | Implementar construtor (nova opção + leitura de parâmetros GA), switch case e método principal |
| `src/driver.cpp` | Adicionar "GeneticAlgo" como opção válida no `destoryStrategy` e opções CLI para parâmetros do GA |

### 3.2 Alterações Detalhadas

#### 3.2.1 `inc/LNS.h`

**Alterar o enum:**
```cpp
enum destroy_heuristic { RANDOMAGENTS, RANDOMWALK, INTERSECTION, GENETIC_ALGO, DESTORY_COUNT };
```

**Adicionar atributos para parâmetros do GA (seção private):**
```cpp
// Genetic Algorithm parameters (configurable via CLI)
int ga_population_size = 8;
int ga_num_generations = 3;
double ga_mutation_rate = 0.20;
```

**Adicionar declarações de métodos (seção private):**
```cpp
// Genetic Algorithm destroy strategy
bool generateNeighborByGeneticAlgorithm(int population_size = -1, int num_generations = -1, double mutation_rate = -1);

// Métodos auxiliares para população inicial do GA
vector<int> getAgentsByRandomWalkForGA();
vector<int> getAgentsByIntersectionForGA();
vector<int> getAgentsByRandomForGA(int seed);

// Avaliação de fitness
int evaluateFitness(const vector<int>& individual);
```

#### 3.2.2 `src/LNS.cpp` - Construtor

Adicionar nova condição no construtor:
```cpp
else if (destory_name == "GeneticAlgo")
    destroy_strategy = GENETIC_ALGO;
```

Adicionar leitura dos parâmetros do GA no construtor (recebidos via argumentos):
```cpp
// Parâmetros do GA (usar valores passados ou manter defaults)
if (ga_pop_size > 0) ga_population_size = ga_pop_size;
if (ga_gens > 0) ga_num_generations = ga_gens;
if (ga_mut_rate >= 0) ga_mutation_rate = ga_mut_rate;
```

#### 3.2.3 `src/LNS.cpp` - Switch Case no método `run()`

```cpp
case GENETIC_ALGO:
    succ = generateNeighborByGeneticAlgorithm();
    break;
```

#### 3.2.4 `src/LNS.cpp` - Método Principal `generateNeighborByGeneticAlgorithm()`

```cpp
bool LNS::generateNeighborByGeneticAlgorithm(int population_size, int num_generations, double mutation_rate) {
    // Usar parâmetros passados ou defaults do objeto
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

    // 2. Loop evolutivo (3 gerações)
    for (int gen = 0; gen < NUM_GENERATIONS; gen++) {
        // 2a. Avaliar fitness de cada indivíduo
        vector<pair<int, int>> fitness_scores; // (fitness, index)
        for (int i = 0; i < population.size(); i++) {
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
        for (int i = 0; i < (int)parents.size() && i + 1 < (int)parents.size(); i += 2) {
            auto& parent1 = parents[i];
            auto& parent2 = parents[i + 1];
            
            // Ponto de corte
            int cut_point = rand() % min(parent1.size(), parent2.size());
            
            vector<int> child1, child2;
            set<int> child1_set, child2_set;
            
            // Primeira parte do parent1 para child1
            for (int j = 0; j <= cut_point && j < parent1.size(); j++) {
                child1.push_back(parent1[j]);
                child1_set.insert(parent1[j]);
            }
            // Complementar com parent2 (agentes não duplicados)
            for (int j = 0; j < parent2.size() && child1.size() < neighbor_size; j++) {
                if (child1_set.find(parent2[j]) == child1_set.end()) {
                    child1.push_back(parent2[j]);
                    child1_set.insert(parent2[j]);
                }
            }
            
            // Primeira parte do parent2 para child2
            for (int j = 0; j <= cut_point && j < parent2.size(); j++) {
                child2.push_back(parent2[j]);
                child2_set.insert(parent2[j]);
            }
            // Complementar com parent1 (agentes não duplicados)
            for (int j = 0; j < parent1.size() && child2.size() < neighbor_size; j++) {
                if (child2_set.find(parent1[j]) == child2_set.end()) {
                    child2.push_back(parent1[j]);
                    child2_set.insert(parent1[j]);
                }
            }
            
            children.push_back(child1);
            children.push_back(child2);
        }
        
        // 2e. Mutação (taxa configurável) nos filhos
        for (int i = 0; i < (int)children.size(); i++) {
            for (int j = 0; j < (int)children[i].size(); j++) {
                if ((double)rand() / RAND_MAX < MUTATION_RATE) {
                    // Trocar agente por outro aleatório não presente
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
        
        // 2f. Manter população constante: adicionar filhos e remover os 4 piores
        for (auto& child : children) {
            population.push_back(child);
        }
        // Reavaliar fitness de toda a população e remover os piores
        vector<pair<int, int>> full_fitness; // (fitness, index)
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
    for (int i = 0; i < population.size(); i++) {
        int fitness = evaluateFitness(population[i]);
        if (fitness < best_fitness) {
            best_fitness = fitness;
            best_idx = i;
        }
    }

    neighbor.agents = population[best_idx];
    if (neighbor.agents.empty())
        return false;
    return true;
}
```

#### 3.2.5 Métodos Auxiliares para População Inicial

```cpp
vector<int> LNS::getAgentsByRandomWalkForGA() {
    if (neighbor_size >= (int)agents.size()) {
        vector<int> all_agents(agents.size());
        for (int i = 0; i < (int)agents.size(); i++)
            all_agents[i] = i;
        return all_agents;
    }

    int a = findMostDelayedAgent();
    if (a < 0) {
        // fallback: retorna agentes aleatórios
        return getAgentsByRandomForGA(42);
    }

    set<int> neighbors_set;
    neighbors_set.insert(a);
    randomWalk(a, agents[a].path[0].location, 0, neighbors_set, neighbor_size, (int)agents[a].path.size() - 1);
    int count = 0;
    while (neighbors_set.size() < neighbor_size && count < 10) {
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

    return vector<int>(neighbors_set.begin(), neighbors_set.end());
}

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

    if (neighbors_set.size() < neighbor_size) {
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
    if (result.size() > neighbor_size) {
        std::random_shuffle(result.begin(), result.end());
        result.resize(neighbor_size);
    }
    return result;
}

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

#### 3.2.6 Avaliação de Fitness

```cpp
int LNS::evaluateFitness(const vector<int>& individual) {
    // Calcula o sum_of_costs hipotético se replanejarmos esses agentes
    // usando PP (Prioritized Planning)
    
    int current_sum_of_costs = sum_of_costs;
    
    // Salvar caminhos antigos e calcular custo antigo dos agentes selecionados
    vector<Path> old_paths(individual.size());
    int old_individual_cost = 0;
    for (int i = 0; i < (int)individual.size(); i++) {
        old_paths[i] = agents[individual[i]].path;
        old_individual_cost += (int)agents[individual[i]].path.size() - 1;
    }
    
    // Remover caminhos da path_table
    for (int i = 0; i < (int)individual.size(); i++) {
        path_table.deletePath(individual[i], agents[individual[i]].path);
    }
    
    // Replanejar usando PP na ordem em que aparecem
    int new_individual_cost = 0;
    bool success = true;
    int planned_count = 0;
    
    for (int i = 0; i < (int)individual.size(); i++) {
        int id = individual[i];
        Path new_path = agents[id].path_planner.findOptimalPath(path_table);
        if (new_path.empty()) {
            success = false;
            break;
        }
        agents[id].path = new_path;
        new_individual_cost += (int)new_path.size() - 1;
        path_table.insertPath(id, agents[id].path);
        planned_count++;
    }
    
    // Calcular fitness: sum_of_costs - custo_antigo_individual + custo_novo_individual
    int fitness;
    if (success) {
        fitness = current_sum_of_costs - old_individual_cost + new_individual_cost;
    } else {
        fitness = INT_MAX; // penalizar soluções inválidas
    }
    
    // Restaurar estado original: remover novos caminhos e reinserir antigos
    for (int i = 0; i < planned_count; i++) {
        path_table.deletePath(individual[i], agents[individual[i]].path);
    }
    for (int i = 0; i < (int)individual.size(); i++) {
        agents[individual[i]].path = old_paths[i];
        path_table.insertPath(individual[i], agents[individual[i]].path);
    }
    
    return fitness;
}
```

#### 3.2.7 `src/driver.cpp` - Atualizar opções da CLI

Adicionar descrição da nova estratégia e parâmetros do GA:
```cpp
("destoryStrategy", po::value<string>()->default_value("Adaptive"),
        "Heuristics for finding subgroups (Random, RandomWalk, Intersection, GeneticAlgo, Adaptive)")
("gaPopSize", po::value<int>()->default_value(8),
        "Population size for Genetic Algorithm destroy strategy")
("gaGenerations", po::value<int>()->default_value(3),
        "Number of generations for Genetic Algorithm destroy strategy")
("gaMutationRate", po::value<double>()->default_value(0.20),
        "Mutation rate for Genetic Algorithm destroy strategy (0.0 to 1.0)")
```

Passar os parâmetros ao construtor do LNS:
```cpp
int ga_pop_size = vm["gaPopSize"].as<int>();
int ga_generations = vm["gaGenerations"].as<int>();
double ga_mutation_rate = vm["gaMutationRate"].as<double>();
```

---

## 4. Considerações Importantes

### Performance
- O `evaluateFitness` realiza replaneamento completo dos agentes, o que é custoso. Com população padrão de 8 e 3 gerações, há múltiplas avaliações de fitness por iteração LNS. Os parâmetros são configuráveis via CLI para ajuste de performance.
- O método restaura o estado da `path_table` após cada avaliação para manter consistência.
- A população é mantida constante: a cada geração, 4 filhos são gerados e os 4 piores indivíduos (entre pais + filhos) são removidos.

### Correção
- Os indivíduos não podem ter agentes duplicados (usar `set` durante crossover).
- O tamanho de cada indivíduo deve ser `neighbor_size`.
- Após selecionar o melhor indivíduo, o fluxo normal do LNS continua (destroy + replan no `run()`).

### Isolamento de Estado (Variáveis Copiadas)

Para garantir que a avaliação de um indivíduo não afete outros indivíduos nem o estado global do LNS, as seguintes variáveis devem ser copiadas localmente:

| Variável | Função | Motivo da Cópia |
|----------|--------|----------------|
| `path_table` | `evaluateFitness` | O replaneamento PP remove e insere caminhos na tabela. Sem cópia local, uma avaliação modifica a tabela global, corrompendo avaliações subsequentes. |
| `agents[].path` | `evaluateFitness` | O replaneamento sobrescreve os caminhos dos agentes. Usar cópia local (`new_paths` vector) evita modificar os paths globais. |
| `tabu_list` | `getAgentsByRandomWalkForGA` | `findMostDelayedAgent()` insere agentes na tabu_list. Sem save/restore, o GA alteraria o estado da tabu_list usado pelo loop principal do LNS. |

**Abordagem adotada:**
- Em `evaluateFitness`: cria-se `PathTable local_path_table = path_table` e usa-se apenas a cópia local. Os novos caminhos são armazenados em `vector<Path> new_paths` sem sobrescrever `agents[id].path`.
- Em `getAgentsByRandomWalkForGA`: salva-se `tabu_list` antes de chamar `findMostDelayedAgent()` e restaura-se ao final.

### ALNS (Adaptive LNS)
- O novo `GENETIC_ALGO` deve ser contabilizado no `DESTORY_COUNT` para funcionar com ALNS.
- Adicionar caso no `chooseDestroyHeuristicbyALNS()`:
```cpp
case 3 : destroy_strategy = GENETIC_ALGO; break;
```

---

## 5. Ordem de Implementação

1. Alterar `inc/LNS.h` (enum + declarações)
2. Alterar `src/LNS.cpp` (construtor + switch + métodos)
3. Alterar `src/driver.cpp` (descrição CLI)
4. Compilar e testar com: `./lns -m map.map -a agents.scen -k 50 -t 60 --destoryStrategy GeneticAlgo --gaPopSize 8 --gaGenerations 3 --gaMutationRate 0.20`
