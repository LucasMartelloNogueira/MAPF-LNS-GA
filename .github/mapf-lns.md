# MAPF-LNS: Solution Overview

## Purpose of This Document

This document describes the approach proposed in the paper that serves as the foundation for this project.

Its purpose is to provide architectural context so that an AI can quickly understand:

- the problem the algorithm solves;
- the main components of the solution;
- how these components interact;
- which heuristics are part of the algorithm;
- the design decisions made by the authors.

This document **does not explain the general MAPF problem**, its fundamental concepts, or its formal definitions. Those concepts are documented in `mapf.md`.

---

# Reference Paper

**Title**

Anytime Multi-Agent Path Finding via Large Neighborhood Search

**Authors**

- Jiaoyang Li
- Zhe Chen
- Daniel Harabor
- Peter J. Stuckey
- Sven Koenig

**Year**

2022

**Published in**

Proceedings of the Thirty-First International Joint Conference on Artificial Intelligence (IJCAI 2022)

---

# Motivation

Existing MAPF algorithms generally fall into one of the following categories:

- optimal algorithms;
- bounded-suboptimal algorithms;
- unbounded-suboptimal algorithms.

Each category has its own strengths and limitations.

## Optimal Algorithms

Optimal algorithms always produce optimal solutions.

However, they have limited scalability and become impractical for problems involving hundreds or thousands of agents.

---

## Bounded-Suboptimal Algorithms

Bounded-suboptimal algorithms produce near-optimal solutions with theoretical quality guarantees.

They scale better than optimal algorithms but are still limited when solving large instances.

---

## Unbounded-Suboptimal Algorithms

Unbounded-suboptimal algorithms scale to thousands of agents and find solutions quickly.

However, they often produce solutions that are significantly far from optimal.

---

# Main Idea

The paper proposes an **anytime** algorithm called **MAPF-LNS**, based on **Large Neighborhood Search (LNS)**.

The key idea is to combine the best of both worlds:

1. quickly generate an initial solution using any existing MAPF algorithm;
2. continuously improve that solution as long as computation time remains available.

Instead of replanning every agent, the algorithm modifies only a small subset of agents at each iteration.

This process is repeated until the time limit is reached.

---

# Overall Algorithm Workflow

The algorithm performs the following cycle:

1. Compute an initial solution using an existing MAPF algorithm.
2. Select a subset of agents (Destroy).
3. Remove the current paths of those agents.
4. Replan only the selected subset (Repair), while keeping the paths of all other agents fixed.
5. Compare the new solution with the current one.
6. If the new solution reduces the Sum-of-Costs, replace the current solution.
7. Repeat until the time limit is reached.

Therefore, the algorithm is an **iterative solution improvement algorithm**.

---

# Large Neighborhood Search for MAPF

Large Neighborhood Search consists of two main phases.

## Destroy

A subset of agents is selected.

The current paths of these agents are discarded.

The paths of all remaining agents are kept fixed and treated as moving obstacles during replanning.

---

## Repair

New paths are computed only for the selected subset.

Any MAPF algorithm capable of handling moving obstacles can be used during this phase.

The authors evaluate several algorithms, including:

- Prioritized Planning (PP)
- EECBS
- CBS

The experimental results show that **Prioritized Planning (PP)** is the most effective repair operator, mainly due to its speed, which allows significantly more LNS iterations within the available time.

---

# Neighborhood Selection Heuristics (Destroy)

The performance of LNS strongly depends on how the subset of agents is selected.

The paper proposes three neighborhood selection heuristics.

---

## 1. Agent-Based Neighborhood

**Objective**

Select agents that are likely blocking other agents.

### Approach

The heuristic starts by selecting an agent whose path has the highest delay.

From this agent, it performs a **restricted random walk**.

During this walk, it identifies the agents preventing the initial agent from following a shorter path.

These agents are added to the neighborhood.

The process continues until the desired neighborhood size is reached.

### Motivation

Agents that interact directly are more likely to benefit from being replanned together.

---

## 2. Map-Based Neighborhood

**Objective**

Select agents that share important regions of the map.

### Approach

The algorithm randomly selects an intersection vertex (degree ≥ 3).

It then:

- performs a Breadth-First Search (BFS);
- explores nearby intersections;
- collects agents whose paths pass through those intersections.

### Motivation

The order in which agents traverse intersections can significantly affect the total solution cost.

This heuristic is specifically designed to exploit that observation.

---

## 3. Random Neighborhood

**Objective**

Provide a simple and diverse exploration strategy.

### Approach

A random subset of agents is selected uniformly.

Although simple, this heuristic performs surprisingly well in highly congested environments.

---

# Adaptive Large Neighborhood Search (ALNS)

Instead of always using the same destroy heuristic, the authors employ **Adaptive Large Neighborhood Search (ALNS)**.

Each heuristic is assigned a weight.

Initially:

- all heuristics have equal weights.

At each iteration:

1. a heuristic is selected using **Roulette Wheel Selection**;
2. the selected heuristic generates a neighborhood;
3. replanning is performed;
4. if the solution improves, the heuristic's weight is increased proportionally to the achieved improvement.

As execution progresses, heuristics that consistently produce better improvements are selected more frequently.

---

# Role of External Algorithms

MAPF-LNS does not replace existing MAPF algorithms.

Instead, it acts as an optimization layer on top of them.

Existing MAPF algorithms can be used in two different stages.

## Initial Solution

Examples:

- PP
- PPS
- EECBS

**Objective**

Generate a feasible initial solution as quickly as possible.

---

## Repair Operator

Examples:

- PP
- CBS
- EECBS

**Objective**

Replan only the selected subset of agents.

The experimental results show that PP offers the best trade-off because of its low execution time.

---

# Main Experimental Results

The experiments were conducted on several standard MAPF benchmark maps, including empty grids, random maps, warehouse environments, and urban maps.

The main findings are summarized below.

## Fast Initial Solution

The algorithm is able to generate initial solutions quickly, even for very large instances.

---

## Significant Cost Reduction

Within 60 seconds, the algorithm reduces the Sum-of-Costs by as much as **36×** compared to the initial solution.

---

## Near-Optimal Solutions

For instances where the optimal solution is known:

- MAPF-LNS found the optimal solution in most cases;
- otherwise, it remained within approximately **1.35%** of the optimum in the worst observed case.

---

## Outperforming Other Anytime Algorithms

The paper compares MAPF-LNS against:

- Anytime BCBS
- Anytime EECBS

The results show that MAPF-LNS achieves:

- higher success rates;
- shorter time to obtain the first solution;
- much faster improvement of solution quality;
- significantly more iterations due to the low computational cost of the repair operator.

---

## Importance of ALNS

No single neighborhood selection heuristic consistently outperforms the others across all scenarios.

Adaptive Large Neighborhood Search provides more robust performance by automatically adapting the heuristic selection strategy to the characteristics of the instance being solved.

---

# Key Insights from the Paper

The authors conclude that:

- finding an initial solution quickly is more important than finding a high-quality initial solution;
- many small local improvements outperform a few large global optimizations;
- replanning only strongly coupled agents is significantly more efficient than replanning all agents;
- the effectiveness of the repair operator depends more on its execution speed than on the optimality of each individual replanning step;
- combining LNS with ALNS enables high-quality solutions for instances that are far larger than those tractable by optimal algorithms.

---

# Role of This Algorithm in This Project

The codebase of this repository implements the **MAPF-LNS** algorithm described in the reference paper.

When implementing new features or modifying the existing implementation, the following design principles should be preserved whenever possible:

- the Large Neighborhood Search architecture;
- the Destroy → Repair workflow;
- the independence of neighborhood selection heuristics;
- the anytime nature of the algorithm, allowing continuous solution improvement throughout execution.