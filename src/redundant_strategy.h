#ifndef REDUNDANT_STRATEGY_H_INCL
#define REDUNDANT_STRATEGY_H_INCL

#include <vector>
#include "instruments.h"
#include "strategy.h"

class RedundantStrategy : public Strategy {
  public:
    RedundantStrategy(const instruments_strategy_t strategies[], 
                      size_t num_strategies);
    
    virtual double calculateTime();
    virtual double calculateCost();
    virtual bool isRedundant() { return true; }

  private:
    std::vector<Strategy *> strategies;
};

#endif
