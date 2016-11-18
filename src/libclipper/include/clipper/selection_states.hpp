//
//  selection_states.hpp
//  Clipper
//
//  Created by YUJIA LUO on 11/18/16.
//
//

#ifndef selection_states_hpp
#define selection_states_hpp

#include <memory>
#include <stdio.h>
#include <stdlib.h>

#include "datatypes.hpp"

namespace clipper {
  class Exp3State {
  public:
    Exp3State() = default;
    Exp3State(std::vector<VersionedModelId> models);
    
    // Each model has a pair (weight, a list of rewards)
    std::unordered_map<VersionedModelId,std::pair<double, std::vector<double>>> model_info_;
  };
  
  class Exp4State {
  public:
    Exp4State() = default;
    Exp4State(std::vector<VersionedModelId> models);
    
    // Each model has a pair (weight, a list of rewards)
    std::unordered_map<VersionedModelId,std::pair<double, std::vector<double>>> model_info_;
    
  };
  
  class EpsilonGreedyState {
  public:
    EpsilonGreedyState() = default;
    EpsilonGreedyState(std::vector<VersionedModelId> models);
    
    // Each model has a pair (expected reward, a list of rewards)
    std::unordered_map<VersionedModelId,std::pair<double, std::vector<double>>> model_info_;
    
    
    
  };
  
  class UCBState {
    
  };
  
}
#endif /* selection_states_hpp */
