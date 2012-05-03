#include "redundant_strategy.h"

RedundantStrategy::RedundantStrategy(const Strategy *strategies[], 
                                     size_t num_strategies)
    : Strategy(NULL, NULL, NULL, NULL)
{
    for (size_t i = 0; i < num_strategies; ++i) {
        this->strategies.push_back(strategies[i]);
    }
}

double 
RedundantStrategy::calculateTime()
{
    Strategy *best_strategy = NULL;
    double best_time = 0.0;
    for (size_t i = 0; i < strategies.size(); ++i) {
        Strategy *strategy = strategies[i];
        double time = strategy->calculateTime();
        if (!best_strategy || time < best_time) {
            best_strategy = strategy;
            best_time = time;
        }
    }
    return best_time;
}

double
RedundantStrategy::calculateCost()
{
    double total_cost = 0.0;
    for (size_t i = 0; i < strategies.size(); ++i) {
        total_cost += strategies[i]->calculateCost();
    }
    return total_cost;
}
