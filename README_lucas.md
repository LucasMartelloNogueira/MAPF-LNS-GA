# Comandos

## remover a cache do build

    rm -rf CMakeCache.txt CMakeFiles/

## rodando experimento

### random-32-32-20

    ./lns -m random-32-32-20.map -a random-32-32-20-random-1.scen -o test2.csv -k 50 -t 60 --destoryStrategy GeneticAlgo --gaPopSize 8 --gaGenerations 3 --gaMutationRate 0.20

### den520d

    ./lns -m maps-scen/den520d/den520d.map -a maps-scen/den520d/den520d.map-scen-random/scen-random/den520d-random-1.scen -o test2.csv -k 50 -t 60 --destoryStrategy GeneticAlgo --gaPopSize 8 --gaGenerations 3 --gaMutationRate 0.20


## criar make 

# informações do código

* onde está a função main?

    R: no arquivo src/driver.cpp

* o que faz o arquivo driver.cpp?

    R: ele é o ponto de entrada da CLI e é responsável por validar os parametros de entrada e de selecionar o solver (que resolver o MAPF) de forma correta

* tem algumas variáveis/atributos ou funções/métodos que estão faltando nos arquivos cpp, onde estão?

    R: provavelmente estão nos header files (path ./inc/), eles não são usados só para declarações, tem implementação também


* onde devo fazer minhas alterações?

    R: começar pelos arquivos LNS.h e LNS.cpp


* onde ficam informações do mapa/grid?

    R: nos arquivos Instance.h e Instance.cpp


## plano para alterações

* arquivo LNS.cpp, no construtor, criar nova opção de destroy_name com valor "genetic_algo", com destroy_name igual ao enum GENETIC_ALGO
* arquivo LNS.cpp, no método run, criar no switch case uma opção para case GENETIC_ALGO
* no arquivo LNS.cpp, criar o método generateNeighborByGeneticAlgorithm

o método generateNeighborByGeneticAlgorithm tem o objetivo deve definir uma lista de agentes na qual deve estar na lista agents da variável neighbor. Para isso, esse método deve:

* usar um algoritmo genético para definir os agentes de neighbor
* selecionar uma população inicial e a cada iteração: avaliar o fitnnes de cada indivíduo, selecionar os mais aptos e fazer crossover por meio de ponto de corte e mutação com taxa de 20%. rode o algoritmo genético um total de 3 vezes
* Uma solução (um individuo da população) é uma lista de agentes (lista de inteiros)
* para obter população inicial, utilize como referencia os métodos do switch case se destroy strategy (método generateNeighborByRandomWalk, método generateNeighborByIntersection e estratégia de agentes aleatórios de RANDOMAGENTS) para escrever outras funções que retornem uma lista de agentes
* crie uma população com 8 soluções: uma obtida a partir do método adaptado de generateNeighborByRandomWalk, uma solução obtida a partir do método adaptado de generateNeighborByIntersection e 6 soluções obtidas de maneira aleatória, mudando o seed na hora de achar cada solução aleatória
* na hora de avaliar a função fitness de cada indivíduo: guarde o sum-of-costa já calculado para todos os agentes, guarde o tamanho dos caminhos atuais dos agentes selecionados na solução, destrua os caminhos dos agentes na solução e gere novos caminhos para esses agentes (na ordem em que aparecem) usando o algortimo PP e guarde o tamanho desses novos caminhos. Depois pegue o sum-of-costs e para cada caminho antigo, diminua o valor da tamanho do caminho, e depois para cada caminho novo, adicione seu valor no sum of costs. Os indivíduos mais aptos são aqueles que possuem o menor sum-of-costs 