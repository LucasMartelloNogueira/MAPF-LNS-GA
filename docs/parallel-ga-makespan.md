# Parallel Genetic Algorithm and Makespan

This feature adds a new destroy strategy named `GeneticAlgoParallel` to the MAPF-LNS solver.

## Strategy

`GeneticAlgoParallel` follows the same evolutionary pipeline already used by `GeneticAlgo`:

- generate an initial population;
- evaluate fitness;
- select parents;
- perform crossover;
- apply mutation;
- keep the best individuals.

The main difference is that the population fitness evaluation is distributed across multiple threads.

## Threads

Use `--num_threads` to control how many threads are used by the parallel genetic algorithm.

Example:

```text
--destoryStrategy GeneticAlgoParallel --num_threads 4
```

The first version prioritizes correctness. The implementation limits the number of workers to the current population size and uses thread-safe local solver instances during fitness evaluation.

## Makespan

In this project, `makespan` is defined as the maximum delay among all agents:

```text
max(agent.getNumOfDelays())
```

This is different from the more common MAPF literature definition based on the largest arrival time.

## ALNS

`GeneticAlgoParallel` is intentionally kept outside ALNS. It is available only through explicit CLI selection and is not part of the adaptive destroy heuristic roulette.