#include "LNS.h"
#include <queue>
#include <random>

LNS::LNS(const Instance& instance, double time_limit, string init_algo_name, string replan_algo_name, string destory_name,
         int neighbor_size, int num_of_iterations, int screen, PIBTPPS_option pipp_option,
         int ga_pop_size, int ga_gens, double ga_mut_rate) :
         instance(instance), time_limit(time_limit), init_algo_name(std::move(init_algo_name)),
         replan_algo_name(replan_algo_name), destory_name(destory_name), neighbor_size(neighbor_size), num_of_iterations(num_of_iterations),
         screen(screen), path_table(instance.map_size),pipp_option(pipp_option), replan_time_limit(time_limit / 100)
{
    start_time = Time::now();
    if (destory_name == "Adaptive")
    {
        ALNS = true;
        destroy_weights.assign((DESTORY_COUNT - 1) * num_neighbor_sizes, 1);
    }
    else if (destory_name == "RandomWalk")
        destroy_strategy = RANDOMWALK;
    else if (destory_name == "Intersection")
        destroy_strategy = INTERSECTION;
    else if (destory_name == "Random")
        destroy_strategy = RANDOMAGENTS;
    else if (destory_name == "GeneticAlgo")
        destroy_strategy = GENETIC_ALGO;
    else
    {
        cerr << "Destroy heuristic " << destory_name << " does not exists. " << endl;
        exit(-1);
    }

    // Set GA parameters from CLI
    if (ga_pop_size > 0) ga_population_size = ga_pop_size;
    if (ga_gens > 0) ga_num_generations = ga_gens;
    if (ga_mut_rate >= 0) ga_mutation_rate = ga_mut_rate;

    int N = instance.getDefaultNumberOfAgents();
    agents.reserve(N);
    for (int i = 0; i < N; i++)
        agents.emplace_back(instance, i);
    preprocessing_time = ((fsec)(Time::now() - start_time)).count();
    if (screen >= 2)
        cout << "Pre-processing time = " << preprocessing_time << " seconds." << endl;
}

bool LNS::run()
{
    success = false;

    // only for statistic analysis, and thus is not included in runtime
    sum_of_distances = 0;
    for (const auto & agent : agents)
    {
        sum_of_distances += agent.path_planner.my_heuristic[agent.path_planner.start_location];
    }

    initial_solution_runtime = 0;
    bool succ = false;
    int count = 0;
    start_time = Time::now();
    while (!succ && initial_solution_runtime < time_limit)
    {
        succ = getInitialSolution();
        initial_solution_runtime = ((fsec)(Time::now() - start_time)).count();
        count++;
    }
    iteration_stats.emplace_back(neighbor.agents.size(),
                                 initial_sum_of_costs, initial_solution_runtime, init_algo_name);
    runtime = initial_solution_runtime;
    if (succ)
    {
        if (screen >= 1)
            cout << "Initial solution cost = " << initial_sum_of_costs << ", "
                 << "runtime = " << initial_solution_runtime << endl;
    }
    else
    {
        cout << "Failed to find an initial solution in "
             << runtime << " seconds and  " << count << " iterations" << endl;
        return false; // terminate because no initial solution is found
    }

    while (runtime < time_limit && iteration_stats.size() <= num_of_iterations)
    {
        runtime =((fsec)(Time::now() - start_time)).count();
        if(screen >= 1)
            validateSolution();
        if (ALNS)
            chooseDestroyHeuristicbyALNS();

        switch (destroy_strategy)
        {
            case RANDOMWALK:
                succ = generateNeighborByRandomWalk();
                break;
            case INTERSECTION:
                succ = generateNeighborByIntersection();
                break;
            case RANDOMAGENTS:
                neighbor.agents.resize(agents.size());
                for (int i = 0; i < (int)agents.size(); i++)
                    neighbor.agents[i] = i;
                if (neighbor.agents.size() > neighbor_size)
                {
                    std::random_shuffle(neighbor.agents.begin(), neighbor.agents.end());
                    neighbor.agents.resize(neighbor_size);
                }
                succ = true;
                break;
            case GENETIC_ALGO:
                succ = generateNeighborByGeneticAlgorithm();
                break;
            default:
                cerr << "Wrong neighbor generation strategy" << endl;
                exit(-1);
        }
        if(!succ)
            continue;

        // store the neighbor information
        neighbor.old_paths.resize(neighbor.agents.size());
        neighbor.old_sum_of_costs = 0;
        for (int i = 0; i < (int)neighbor.agents.size(); i++)
        {
            if (replan_algo_name == "PP")
                neighbor.old_paths[i] = agents[neighbor.agents[i]].path;
            path_table.deletePath(neighbor.agents[i], agents[neighbor.agents[i]].path);
            neighbor.old_sum_of_costs += agents[neighbor.agents[i]].path.size() - 1;
        }

        if (replan_algo_name == "EECBS")
            succ = runEECBS();
        else if (replan_algo_name == "CBS")
            succ = runCBS();
        else if (replan_algo_name == "PP")
            succ = runPP();
        else
        {
            cerr << "Wrong replanning strategy" << endl;
            exit(-1);
        }

        if (ALNS) // update destroy heuristics
        {
            if (neighbor.old_sum_of_costs > neighbor.sum_of_costs )
                destroy_weights[selected_neighbor] =
                        reaction_factor * (neighbor.old_sum_of_costs - neighbor.sum_of_costs) / neighbor.agents.size()
                        + (1 - reaction_factor) * destroy_weights[selected_neighbor];
            else
                destroy_weights[selected_neighbor] =
                        (1 - decay_factor) * destroy_weights[selected_neighbor];
        }
        runtime = ((fsec)(Time::now() - start_time)).count();
        sum_of_costs += neighbor.sum_of_costs - neighbor.old_sum_of_costs;
        if (screen >= 1)
            cout << "Iteration " << iteration_stats.size() << ", "
                 << "group size = " << neighbor.agents.size() << ", "
                 << "solution cost = " << sum_of_costs << ", "
                 << "remaining time = " << time_limit - runtime << endl;
        iteration_stats.emplace_back(neighbor.agents.size(), sum_of_costs, runtime, replan_algo_name);
    }


    average_group_size = - iteration_stats.front().num_of_agents;
    for (const auto& data : iteration_stats)
        average_group_size += data.num_of_agents;
    if (average_group_size > 0)
        average_group_size /= (double)(iteration_stats.size() - 1);
    cout << getSolverName() << ": Iterations = " << iteration_stats.size() << ", "
         << "solution cost = " << sum_of_costs << ", "
         << "initial solution cost = " << initial_sum_of_costs << ", "
         << "runtime = " << runtime << ", "
         << "group size = " << average_group_size << ", "
         << "failed iterations = " << num_of_failures << endl;
    success = true;
    return true;
}


bool LNS::getInitialSolution()
{
    neighbor.agents.resize(agents.size());
    for (int i = 0; i < (int)agents.size(); i++)
        neighbor.agents[i] = i;
    neighbor.old_sum_of_costs = MAX_COST;
    neighbor.sum_of_costs = 0;
    bool succ = false;
    if (init_algo_name == "EECBS")
        succ = runEECBS();
    else if (init_algo_name == "PP")
        succ = runPP();
    else if (init_algo_name == "PIBT")
        succ = runPIBT();
    else if (init_algo_name == "PPS")
        succ = runPPS();
    else if (init_algo_name == "winPIBT")
        succ = runWinPIBT();
    else if (init_algo_name == "CBS")
        succ = runCBS();
    else
    {
        cerr <<  "Initial MAPF solver " << init_algo_name << " does not exist!" << endl;
        exit(-1);
    }
    if (succ)
    {
        initial_sum_of_costs = neighbor.sum_of_costs;
        sum_of_costs = neighbor.sum_of_costs;
        return true;
    }
    else
    {
        return false;
    }

}

bool LNS::runEECBS()
{
    vector<SingleAgentSolver*> search_engines;
    search_engines.reserve(neighbor.agents.size());
    for (int i : neighbor.agents)
    {
        search_engines.push_back(&agents[i].path_planner);
    }

    ECBS ecbs(search_engines, path_table, screen - 1);
    ecbs.setPrioritizeConflicts(true);
    ecbs.setDisjointSplitting(false);
    ecbs.setBypass(true);
    ecbs.setRectangleReasoning(true);
    ecbs.setCorridorReasoning(true);
    ecbs.setHeuristicType(heuristics_type::WDG, heuristics_type::GLOBAL);
    ecbs.setTargetReasoning(true);
    ecbs.setMutexReasoning(false);
    ecbs.setConflictSelectionRule(conflict_selection::EARLIEST);
    ecbs.setNodeSelectionRule(node_selection::NODE_CONFLICTPAIRS);
    ecbs.setSavingStats(false);
    double w;
    if (iteration_stats.empty())
        w = 2; // initial run
    else
        w = 1.1; // replan
    ecbs.setHighLevelSolver(high_level_solver_type::EES, w);
    runtime = ((fsec)(Time::now() - start_time)).count();
    double T = time_limit - runtime;
    if (!iteration_stats.empty()) // replan
        T = min(T, replan_time_limit);
    bool succ = ecbs.solve(T, 0);
    if (succ && ecbs.solution_cost < neighbor.old_sum_of_costs) // accept new paths
    {
        auto id = neighbor.agents.begin();
        for (size_t i = 0; i < neighbor.agents.size(); i++)
        {
            agents[*id].path = *ecbs.paths[i];
            path_table.insertPath(agents[*id].id, agents[*id].path);
            ++id;
        }
        neighbor.sum_of_costs = ecbs.solution_cost;
        if (sum_of_costs_lowerbound < 0)
            sum_of_costs_lowerbound = ecbs.getLowerBound();
    }
    else // stick to old paths
    {
        if (!neighbor.old_paths.empty())
        {
            for (int id : neighbor.agents)
            {
                path_table.insertPath(agents[id].id, agents[id].path);
            }
            neighbor.sum_of_costs = neighbor.old_sum_of_costs;
        }
        if (!succ)
            num_of_failures++;
    }
    return succ;
}

bool LNS::runCBS()
{
    if (screen >= 2)
        cout << "old sum of costs = " << neighbor.old_sum_of_costs << endl;
    vector<SingleAgentSolver*> search_engines;
    search_engines.reserve(neighbor.agents.size());
    for (int i : neighbor.agents)
    {
        search_engines.push_back(&agents[i].path_planner);
    }

    CBS cbs(search_engines, path_table, screen - 1);
    cbs.setPrioritizeConflicts(true);
    cbs.setDisjointSplitting(false);
    cbs.setBypass(true);
    cbs.setRectangleReasoning(true);
    cbs.setCorridorReasoning(true);
    cbs.setHeuristicType(heuristics_type::WDG, heuristics_type::ZERO);
    cbs.setTargetReasoning(true);
    cbs.setMutexReasoning(false);
    cbs.setConflictSelectionRule(conflict_selection::EARLIEST);
    cbs.setNodeSelectionRule(node_selection::NODE_CONFLICTPAIRS);
    cbs.setSavingStats(false);
    cbs.setHighLevelSolver(high_level_solver_type::ASTAR, 1);
    runtime = ((fsec)(Time::now() - start_time)).count();
    double T = time_limit - runtime; // time limit
    if (!iteration_stats.empty()) // replan
        T = min(T, replan_time_limit);
    bool succ = cbs.solve(T, 0);
    if (succ && cbs.solution_cost < neighbor.old_sum_of_costs) // accept new paths
    {
        auto id = neighbor.agents.begin();
        for (size_t i = 0; i < neighbor.agents.size(); i++)
        {
            agents[*id].path = *cbs.paths[i];
            path_table.insertPath(agents[*id].id, agents[*id].path);
            ++id;
        }
        neighbor.sum_of_costs = cbs.solution_cost;
        if (sum_of_costs_lowerbound < 0)
            sum_of_costs_lowerbound = cbs.getLowerBound();
    }
    else // stick to old paths
    {
        if (!neighbor.old_paths.empty())
        {
            for (int id : neighbor.agents)
            {
                path_table.insertPath(agents[id].id, agents[id].path);
            }
            neighbor.sum_of_costs = neighbor.old_sum_of_costs;

        }
        if (!succ)
            num_of_failures++;
    }
    return succ;
}

bool LNS::runPP()
{
    auto shuffled_agents = neighbor.agents;
    std::random_shuffle(shuffled_agents.begin(), shuffled_agents.end());
    if (screen >= 2) {
        for (auto id : shuffled_agents)
            cout << id << "(" << agents[id].path_planner.my_heuristic[agents[id].path_planner.start_location] <<
                "->" << agents[id].path.size() - 1 << "), ";
        cout << endl;
    }
    int remaining_agents = (int)shuffled_agents.size();
    auto p = shuffled_agents.begin();
    neighbor.sum_of_costs = 0;
    runtime = ((fsec)(Time::now() - start_time)).count();
    double T = time_limit - runtime; // time limit
    if (!iteration_stats.empty()) // replan
        T = min(T, replan_time_limit);
    auto time = Time::now();
    while (p != shuffled_agents.end() && ((fsec)(Time::now() - time)).count() < T)
    {
        int id = *p;
        if (screen >= 3)
            cout << "Remaining agents = " << remaining_agents <<
                 ", remaining time = " << time_limit - runtime << " seconds. " << endl
                 << "Agent " << agents[id].id << endl;
        agents[id].path = agents[id].path_planner.findOptimalPath(path_table);
        if (agents[id].path.empty())
        {
            break;
        }
        neighbor.sum_of_costs += (int)agents[id].path.size() - 1;
        if (neighbor.sum_of_costs >= neighbor.old_sum_of_costs)
            break;
        path_table.insertPath(agents[id].id, agents[id].path);
        remaining_agents--;
        ++p;
    }
    if (p == shuffled_agents.end() && neighbor.sum_of_costs < neighbor.old_sum_of_costs) // accept new paths
    {
        return true;
    }
    else // stick to old paths
    {
        if (p != shuffled_agents.end())
            num_of_failures++;
        auto p2 = shuffled_agents.begin();
        while (p2 != p)
        {
            int a = *p2;
            path_table.deletePath(agents[a].id, agents[a].path);
            ++p2;
        }
        if (!neighbor.old_paths.empty())
        {
            p2 = neighbor.agents.begin();
            for (int i = 0; i < (int)neighbor.agents.size(); i++)
            {
                int a = *p2;
                agents[a].path = neighbor.old_paths[i];
                path_table.insertPath(agents[a].id, agents[a].path);
                ++p2;
            }
            neighbor.sum_of_costs = neighbor.old_sum_of_costs;
        }
        return false;
    }
}

bool LNS::runPPS(){
    auto shuffled_agents = neighbor.agents;
    std::random_shuffle(shuffled_agents.begin(), shuffled_agents.end());

    MAPF P = preparePIBTProblem(shuffled_agents);
    P.setTimestepLimit(pipp_option.timestepLimit);

    // seed for solver
    std::mt19937* MT_S = new std::mt19937(0);
    PPS solver(&P,MT_S);
    solver.setTimeLimit(time_limit);
    // solver.WarshallFloyd();
    bool result = solver.solve();
    if (result)
        updatePIBTResult(P.getA(),shuffled_agents);
    return result;
}

bool LNS::runPIBT(){
    auto shuffled_agents = neighbor.agents;
     std::random_shuffle(shuffled_agents.begin(), shuffled_agents.end());

    MAPF P = preparePIBTProblem(shuffled_agents);

    // seed for solver
    std::mt19937* MT_S = new std::mt19937(0);
    PIBT solver(&P,MT_S);
    solver.setTimeLimit(time_limit);
    bool result = solver.solve();
    if (result)
        updatePIBTResult(P.getA(),shuffled_agents);
    return result;
}

bool LNS::runWinPIBT(){
    auto shuffled_agents = neighbor.agents;
    std::random_shuffle(shuffled_agents.begin(), shuffled_agents.end());

    MAPF P = preparePIBTProblem(shuffled_agents);
    P.setTimestepLimit(pipp_option.timestepLimit);

    // seed for solver
    std::mt19937* MT_S = new std::mt19937(0);
    winPIBT solver(&P,pipp_option.windowSize,pipp_option.winPIBTSoft,MT_S);
    solver.setTimeLimit(time_limit);
    bool result = solver.solve();
    if (result)
        updatePIBTResult(P.getA(),shuffled_agents);
    return result;
}

MAPF LNS::preparePIBTProblem(vector<int> shuffled_agents){

    // seed for problem and graph
    std::mt19937* MT_PG = new std::mt19937(0);

    // Graph* G = new SimpleGrid(instance);
    Graph* G = new SimpleGrid(instance.getMapFile());

    std::vector<Task*> T;
    PIBT_Agents A;

    for (int i : shuffled_agents){
        assert(G->existNode(agents[i].path_planner.start_location));
        assert(G->existNode(agents[i].path_planner.goal_location));
        PIBT_Agent* a = new PIBT_Agent(G->getNode( agents[i].path_planner.start_location));

    //  PIBT_Agent* a = new PIBT_Agent(G->getNode( agents[i].path_planner.start_location));
        A.push_back(a);
        Task* tau = new Task(G->getNode( agents[i].path_planner.goal_location));


        T.push_back(tau);
        if(screen>=5){
            cout<<"Agent "<<i<<" start: " <<a->getNode()->getPos()<<" goal: "<<tau->getG().front()->getPos()<<endl;
        }
    }

    return MAPF(G, A, T, MT_PG);

}

void LNS::updatePIBTResult(const PIBT_Agents& A,vector<int> shuffled_agents){
    int soc = 0;
    for (int i=0; i<A.size();i++){
        int a_id = shuffled_agents[i];

        agents[a_id].path.resize(A[i]->getHist().size());
        int last_goal_visit = 0;
        if(screen>=2)
            std::cout<<A[i]->logStr()<<std::endl;
        for (int n_index = 0; n_index < A[i]->getHist().size(); n_index++){
            auto n = A[i]->getHist()[n_index];
            agents[a_id].path[n_index] = PathEntry(n->v->getId());

            //record the last time agent reach the goal from a non-goal vertex.
            if(agents[a_id].path_planner.goal_location == n->v->getId() 
                && n_index - 1>=0
                && agents[a_id].path_planner.goal_location !=  agents[a_id].path[n_index - 1].location)
                last_goal_visit = n_index;

        }
        //resize to last goal visit time
        agents[a_id].path.resize(last_goal_visit + 1);
        if(screen>=2)
            std::cout<<" Length: "<< agents[a_id].path.size() <<std::endl;
        if(screen>=5){
            cout <<"Agent "<<a_id<<":";
            for (auto loc : agents[a_id].path){
                cout <<loc.location<<",";
            }
            cout<<endl;
        }
        path_table.insertPath(agents[a_id].id, agents[a_id].path);
        soc += agents[a_id].path.size()-1;
    }

    neighbor.sum_of_costs =soc;
}

void LNS::chooseDestroyHeuristicbyALNS(){
    double sum = 0;
    for (const auto& h : destroy_weights)
        sum += h;
    if (screen >= 2)
    {
        cout << "destroy weights = ";
        for (const auto& h : destroy_weights)
            cout << h / sum << ",";
    }
    double r = (double) rand() / RAND_MAX;
    double threshold = destroy_weights[0];
    selected_neighbor = 0;
    while (threshold < r * sum)
    {
        selected_neighbor++;
        threshold += destroy_weights[selected_neighbor];
    }
    switch (selected_neighbor / num_neighbor_sizes)
    {
        case 0 : destroy_strategy = RANDOMWALK; break;
        case 1 : destroy_strategy = INTERSECTION; break;
        case 2 : destroy_strategy = RANDOMAGENTS; break;
        default : cerr << "ERROR" << endl; exit(-1);
    }
    // neighbor_size = (int) pow(2, selected_neighbor % num_neighbor_sizes + 1);
}

bool LNS::generateNeighborByIntersection(bool temporal){
    if (intersections.empty())
    {
        for (int i = 0; i < instance.map_size; i++)
        {
            if (!instance.isObstacle(i) && instance.getDegree(i) > 2)
                intersections.push_back(i);
        }
    }

    set<int> neighbors_set;
    /*int location = -1;
    auto intersection_copy = intersections;
    while (neighbors_set.size() <= 1 && !intersection_copy.empty())
    {
        neighbors_set.clear();
        auto pt = intersection_copy.begin();
        std::advance(pt, rand() % intersection_copy.size());
        location = *pt;
        if (temporal)
        {
            path_table.get_agents(neighbors_set, neighbor_size, location);
        }
        else
        {
            path_table.get_agents(neighbors_set, location);
        }
        intersection_copy.erase(pt);
    }
    if (neighbors_set.size() <= 1)
        return false;*/
    auto pt = intersections.begin();
    std::advance(pt, rand() % intersections.size());
    int location = *pt;
    path_table.get_agents(neighbors_set, neighbor_size, location);
    if (neighbors_set.size() < neighbor_size)
    {
        set<int> closed;
        closed.insert(location);
        std::queue<int> open;
        open.push(location);
        while (!open.empty() && (int) neighbors_set.size() < neighbor_size)
        {
            int curr = open.front();
            open.pop();
            for (auto next : instance.getNeighbors(curr))
            {
                if (closed.count(next) > 0)
                    continue;
                open.push(next);
                closed.insert(next);
                if (instance.getDegree(next) >= 3)
                {
                    path_table.get_agents(neighbors_set, neighbor_size, next);
                    if ((int) neighbors_set.size() == neighbor_size)
                        break;
                }
            }
        }
    }
    neighbor.agents.assign(neighbors_set.begin(), neighbors_set.end());
    if (neighbor.agents.size() > neighbor_size)
    {
        std::random_shuffle(neighbor.agents.begin(), neighbor.agents.end());
        neighbor.agents.resize(neighbor_size);
    }
    if (screen >= 2)
        cout << "Generate " << neighbor.agents.size() << " neighbors by intersection " << location << endl;
    return true;
}

bool LNS::generateNeighborByRandomWalk(){
    if (neighbor_size >= (int)agents.size())
    {
        neighbor.agents.resize(agents.size());
        for (int i = 0; i < (int)agents.size(); i++)
            neighbor.agents[i] = i;
        return true;
    }

    int a = findMostDelayedAgent();
    if (a < 0)
        return false;
    
    set<int> neighbors_set;
    neighbors_set.insert(a);
    randomWalk(a, agents[a].path[0].location, 0, neighbors_set, neighbor_size, (int) agents[a].path.size() - 1);
    int count = 0;
    while (neighbors_set.size() < neighbor_size && count < 10)
    {
        int t = rand() % agents[a].path.size();
        randomWalk(a, agents[a].path[t].location, t, neighbors_set, neighbor_size, (int) agents[a].path.size() - 1);
        count++;
        // select the next agent randomly
        int idx = rand() % neighbors_set.size();
        int i = 0;
        for (auto n : neighbors_set)
        {
            if (i == idx)
            {
                a = n;
                break;
            }
            i++;
        }
    }
    if (neighbors_set.size() < 2)
        return false;
    neighbor.agents.assign(neighbors_set.begin(), neighbors_set.end());
    if (screen >= 2)
        cout << "Generate " << neighbor.agents.size() << " neighbors by random walks of agent " << a
             << "(" << agents[a].path_planner.my_heuristic[agents[a].path_planner.start_location]
             << "->" << agents[a].path.size() - 1 << ")" << endl;

    return true;
}


bool LNS::generateNeighborByGeneticAlgorithm(int population_size, int num_generations, double mutation_rate) {
    const int POP_SIZE = (population_size > 0) ? population_size : ga_population_size;
    const int NUM_GENERATIONS = (num_generations > 0) ? num_generations : ga_num_generations;
    const double MUTATION_RATE = (mutation_rate >= 0) ? mutation_rate : ga_mutation_rate;

    vector<vector<int>> population;
    population.push_back(getAgentsByRandomWalkForGA());
    population.push_back(getAgentsByIntersectionForGA());
    for (int seed = 0; seed < POP_SIZE - 2; seed++) {
        population.push_back(getAgentsByRandomForGA(seed));
    }

    if (population.empty())
        return false;

    int best_idx = 0;
    for (int gen = 0; gen <= NUM_GENERATIONS; gen++) {
        vector<pair<int, int>> fitness_scores;
        fitness_scores.reserve(population.size());
        for (int i = 0; i < (int)population.size(); i++) {
            int fitness = evaluateFitness(population[i]);
            fitness_scores.push_back({fitness, i});
        }
        sort(fitness_scores.begin(), fitness_scores.end());
        best_idx = fitness_scores.front().second;

        if (gen == NUM_GENERATIONS)
            break;

        const int child_count = min(4, (int)population.size() / 2);
        for (int i = 0; i < child_count; i++) {
            const auto& parent_best = population[fitness_scores[i].second];
            const int worst_idx = fitness_scores[fitness_scores.size() - 1 - i].second;
            const auto& parent_worst = population[worst_idx];

            vector<int> child;
            set<int> child_set;
            int min_parent_size = min(parent_best.size(), parent_worst.size());
            int cut_point = (min_parent_size > 0) ? rand() % min_parent_size : -1;

            for (int j = 0; j <= cut_point && j < (int)parent_best.size(); j++) {
                child.push_back(parent_best[j]);
                child_set.insert(parent_best[j]);
            }
            for (int j = 0; j < (int)parent_worst.size() && (int)child.size() < neighbor_size; j++) {
                if (child_set.find(parent_worst[j]) == child_set.end()) {
                    child.push_back(parent_worst[j]);
                    child_set.insert(parent_worst[j]);
                }
            }
            population[worst_idx] = child;
        }

        for (int i = 0; i < (int)population.size(); i++) {
            if (i == best_idx)
                continue;

            set<int> current_set(population[i].begin(), population[i].end());
            for (int j = 0; j < (int)population[i].size(); j++) {
                if ((double)rand() / RAND_MAX < MUTATION_RATE) {
                    int previous_agent = population[i][j];
                    current_set.erase(previous_agent);
                    int new_agent = rand() % agents.size();
                    int attempts = 0;
                    while (current_set.count(new_agent) > 0 && attempts < 20) {
                        new_agent = rand() % agents.size();
                        attempts++;
                    }
                    if (current_set.count(new_agent) == 0) {
                        population[i][j] = new_agent;
                        current_set.insert(new_agent);
                    }
                    else {
                        current_set.insert(previous_agent);
                    }
                }
            }
        }
    }

    neighbor.agents = population[best_idx];
    if (neighbor.agents.empty())
        return false;

    if (screen >= 2)
        cout << "Generate " << neighbor.agents.size() << " neighbors by genetic algorithm" << endl;
    return true;
}


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

vector<int> LNS::getAgentsByRandomForGA(int seed) {
    std::mt19937 rng(seed);
    vector<int> all_agents(agents.size());
    for (int i = 0; i < (int)agents.size(); i++)
        all_agents[i] = i;
    std::shuffle(all_agents.begin(), all_agents.end(), rng);
    all_agents.resize(min((int)all_agents.size(), neighbor_size));
    return all_agents;
}

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

int LNS::findMostDelayedAgent(){
    int a = -1;
    int max_delays = -1;
    for (int i = 0; i < agents.size(); i++)
    {
        if (tabu_list.find(i) != tabu_list.end())
            continue;
        int delays = agents[i].getNumOfDelays();
        if (max_delays < delays)
        {
            a = i;
            max_delays = delays;
        }
    }
    if (max_delays == 0)
    {
        tabu_list.clear();
        return -1;
    }
    tabu_list.insert(a);
    if (tabu_list.size() == agents.size())
        tabu_list.clear();
    return a;
}

int LNS::findRandomAgent() const{
    int a = 0;
    int pt = rand() % (sum_of_costs - sum_of_distances) + 1;
    int sum = 0;
    for (; a < (int) agents.size(); a++)
    {
        sum += agents[a].getNumOfDelays();
        if (sum >= pt)
            break;
    }
    assert(sum >= pt);
    return a;
}

// a random walk with path that is shorter than upperbound and has conflicting with neighbor_size agents
void LNS::randomWalk(int agent_id, int start_location, int start_timestep,
                     set<int>& conflicting_agents, int neighbor_size, int upperbound)
{
    int loc = start_location;
    for (int t = start_timestep; t < upperbound; t++)
    {
        auto next_locs = instance.getNeighbors(loc);
        next_locs.push_back(loc);
        while (!next_locs.empty())
        {
            int step = rand() % next_locs.size();
            auto it = next_locs.begin();
            advance(it, step);
            int next_h_val = agents[agent_id].path_planner.my_heuristic[*it];
            if (t + 1 + next_h_val < upperbound) // move to this location
            {
                path_table.getConflictingAgents(agent_id, conflicting_agents, loc, *it, t + 1);
                loc = *it;
                break;
            }
            next_locs.erase(it);
        }
        if (next_locs.empty() || conflicting_agents.size() >= neighbor_size)
            break;
    }
}


void LNS::validateSolution() const {
    int sum = 0;
    for (const auto& a1_ : agents)
    {
        if (a1_.path.empty())
        {
            cerr << "No solution for agent " << a1_.id << endl;
            exit(-1);
        }
        else if (a1_.path_planner.start_location != a1_.path.front().location)
        {
            cerr << "The path of agent " << a1_.id << " starts from location " << a1_.path.front().location
                << ", which is different from its start location " << a1_.path_planner.start_location << endl;
            exit(-1);
        }
        else if (a1_.path_planner.goal_location != a1_.path.back().location)
        {
            cerr << "The path of agent " << a1_.id << " ends at location " << a1_.path.back().location
                 << ", which is different from its goal location " << a1_.path_planner.goal_location << endl;
            exit(-1);
        }
        for (int t = 1; t < (int) a1_.path.size(); t++ )
        {
            if (!instance.validMove(a1_.path[t - 1].location, a1_.path[t].location))
            {
                cerr << "The path of agent " << a1_.id << " jump from "
                     << a1_.path[t - 1].location << " to " << a1_.path[t].location
                     << " between timesteps " << t - 1 << " and " << t << endl;
                exit(-1);
            }
        }
        sum += (int) a1_.path.size() - 1;
        for (const auto& a2_: agents)
        {
            if (a1_.id >= a2_.id || a1_.path.empty())
                continue;
            const auto a1 = a1_.path.size() <= a2_.path.size()? a1_ : a2_;
            const auto a2 = a1_.path.size() <= a2_.path.size()? a2_ : a1_;
            int t = 1;
            for (; t < (int) a1.path.size(); t++)
            {
                if (a1.path[t].location == a2.path[t].location) // vertex conflict
                {
                    cerr << "Find a vertex conflict between agents " << a1.id << " and " << a2.id <<
                            " at location " << a1.path[t].location << " at timestep " << t << endl;
                    exit(-1);
                }
                else if (a1.path[t].location == a2.path[t - 1].location &&
                        a1.path[t - 1].location == a2.path[t].location) // edge conflict
                {
                    cerr << "Find an edge conflict between agents " << a1.id << " and " << a2.id <<
                         " at edge (" << a1.path[t - 1].location << "," << a1.path[t].location <<
                         ") at timestep " << t << endl;
                    exit(-1);
                }
            }
            int target = a1.path.back().location;
            for (; t < (int) a2.path.size(); t++)
            {
                if (a2.path[t].location == target)  // target conflict
                {
                    cerr << "Find a target conflict where agent " << a2.id << " traverses agent " << a1.id <<
                         "'s target location " << target << " at timestep " << t << endl;
                    exit(-1);
                }
            }
        }
    }
    if (sum_of_costs != sum)
    {
        cerr << "The computed sum of costs " << sum_of_costs <<
             " is different from the sum of the paths in the solution " << sum << endl;
        exit(-1);
    }
}

void LNS::writeIterStatsToFile(string file_name) const {
    std::ofstream output;
    output.open(file_name);
    // header
    output << "num of agents," <<
           "sum of costs," <<
           "runtime," <<
           "cost lowerbound," <<
           "sum of distances," <<
           "MAPF algorithm" << endl;

    for (const auto &data : iteration_stats)
    {
        output << data.num_of_agents << "," <<
               data.sum_of_costs << "," <<
               data.runtime << "," <<
               max(sum_of_costs_lowerbound, sum_of_distances) << "," <<
               sum_of_distances << "," <<
               data.algorithm << endl;
    }
    output.close();
}

void LNS::writeResultToFile(string file_name) const {
    const string result_header = "runtime,solution cost,initial solution cost,min f value,root g value,"
                                 "iterations,group size,runtime of initial solution,area under curve,"
                                 "preprocessing runtime,solver name,instance name,"
                                 "agents,destoryStrategy,gaPopSize,gaGenerations,gaMutationRate,"
                                 "failed iterations,neighborSize,success,"
                                 "time limit,initAlgo,replanAlgo";
    std::ifstream infile(file_name);
    bool exist = infile.good();
    if (!exist)
    {
        infile.close();
        ofstream addHeads(file_name);
        addHeads << result_header << endl;
        addHeads.close();
    }
    else
    {
        string header;
        getline(infile, header);
        if (header != result_header)
        {
            int num_existing_columns = header.empty() ? 0 : (int)std::count(header.begin(), header.end(), ',') + 1;
            int num_result_columns = (int)std::count(result_header.begin(), result_header.end(), ',') + 1;
            vector<string> existing_rows;
            string row;
            while (getline(infile, row))
                existing_rows.push_back(row);
            infile.close();

            ofstream updated(file_name);
            updated << result_header << endl;
            for (const auto& existing_row : existing_rows)
            {
                updated << existing_row;
                for (int i = num_existing_columns; !existing_row.empty() && i < num_result_columns; i++)
                    updated << ",-";
                updated << endl;
            }
            updated.close();
        }
        else
        {
            infile.close();
        }
    }
    ofstream stats(file_name, std::ios::app);
    double auc = 0;
    if (!iteration_stats.empty())
    {
        auto prev = iteration_stats.begin();
        auto curr = prev;
        ++curr;
        while (curr != iteration_stats.end() && curr->runtime < time_limit)
        {
            auc += (prev->sum_of_costs - sum_of_distances) * (curr->runtime - prev->runtime);
            prev = curr;
            ++curr;
        }
        auc += (prev->sum_of_costs - sum_of_distances) * (time_limit - prev->runtime);
    }
    stats << runtime << "," << sum_of_costs << "," << initial_sum_of_costs << "," <<
            max(sum_of_distances, sum_of_costs_lowerbound) << "," << sum_of_distances << "," <<
            iteration_stats.size() << "," << average_group_size << "," <<
            initial_solution_runtime << "," << auc << "," <<
            preprocessing_time << "," << getSolverName() << "," << instance.getInstanceName() << "," <<
            instance.getDefaultNumberOfAgents() << "," << destory_name << ",";
    if (destory_name == "GeneticAlgo")
    {
        stats << ga_population_size << "," << ga_num_generations << "," << ga_mutation_rate << ",";
    }
    else
    {
        stats << "-,-,-,";
    }
    stats << num_of_failures << "," << neighbor_size << "," << (success ? "true" : "false") << "," <<
            time_limit << "," << init_algo_name << "," << replan_algo_name << endl;
    stats.close();
}

void LNS::writePathsToFile(string file_name) const {
    std::ofstream output;
    output.open(file_name);
    // header
    output << agents.size() << endl;

    for (const auto &agent : agents)
    {
        for (const auto &state : agent.path)
            output << state.location << ",";
        output << endl;
    }
    output.close();
}
/*
bool LNS::generateNeighborByStart()
{
    if (start_locations.empty())
    {
        for (int i = 0; i < (int)al.agents_all.size(); i++)
        {
            auto start = ml.linearize_coordinate(al.agents_all[i].initial_location);
            start_locations[start].push_back(i);
        }
        auto it = start_locations.begin();
        while(it != start_locations.end()) // delete start locations that have only one agent
        {
            if (it->second.size() == 1)
                it = start_locations.erase(it);
            else
                ++it;
        }
    }
    if (start_locations.empty())
        return false;
    auto step = rand() % start_locations.size();
    auto it = start_locations.begin();
    advance(it, step);
    neighbors.assign(it->second.begin(), it->second.end());
    if (neighbors.size() > max_group_size ||
        (replan_strategy == 0 && neighbors.size() > group_size)) // resize the group for CBS
    {
        sortNeighborsRandomly();
        if (replan_strategy == 0)
            neighbors.resize(group_size);
        else
            neighbors.resize(max_group_size);
    }
    if (options1.debug)
        cout << "Generate " << neighbors.size() << " neighbors by start location " << it->first << endl;
    return true;
}

void LNS::sortNeighborsByRegrets()
{
    quickSort(neighbors, 0, neighbors.size() - 1, true);
    if (options1.debug) {
        for (auto agent : neighbors) {
            cout << agent << "(" << al.agents_all[agent].distance_to_goal << "->" << al.paths_all[agent].size() - 1
                 << "), ";
        }
        cout << endl;
    }
}

void LNS::sortNeighborsByStrategy()
{
    if (agent_priority_strategy == 5)
    {
        // decide the agent priority for agents at the same start location
        start_locations.clear(); // map the agents to their start locations
        for (auto i : neighbors)
            start_locations[ml.linearize_coordinate(al.agents_all[i].initial_location)].push_back(i);
        for (auto& agents : start_locations)
        {
            vector<int> agents_vec(agents.second.begin(), agents.second.end());
            quickSort(agents_vec, 0, agents_vec.size() - 1, false);
            for (int i = 0; i < (int)agents.second.size(); i++)
            {
                al.agents_all[agents_vec[i]].priority = i;
            }
        }
    }

    // sort the agents
    if (agent_priority_strategy != 0)
        quickSort(neighbors, 0, (int)neighbors.size() - 1, false);
}


void LNS::quickSort(vector<int>& agent_order, int low, int high, bool regret)
{
    if (low >= high)
        return;
    int pivot = agent_order[high];    // pivot
    int i = low;  // Index of smaller element
    for (int j = low; j <= high - 1; j++)
    {
        // If current element is smaller than or equal to pivot
        if ((regret && compareByRegrets(agent_order[j], pivot)) ||
            al.compareAgent(al.agents_all[agent_order[j]], al.agents_all[pivot], agent_priority_strategy))
        {
            std::swap(agent_order[i], agent_order[j]);
            i++;    // increment index of smaller element
        }
    }
    std::swap(agent_order[i], agent_order[high]);

    quickSort(agent_order, low, i - 1, regret);  // Before i
    quickSort(agent_order, i + 1, high, regret); // After i
}



*/
