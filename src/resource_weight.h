#ifndef RESOURCE_WEIGHT_H_INCL
#define RESOURCE_WEIGHT_H_INCL

class ResourceWeight {
  public:
    // return the current resource cost weight
    virtual double getWeight() = 0;
    
    // return the resource cost weight assuming I incur 'cost' over 'duration' seconds.
    // the values passed in here are for current cost, not delta cost.
    virtual double getWeight(double cost, double duration) = 0;
};

#endif
