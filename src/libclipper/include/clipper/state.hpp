#ifndef CLIPPER_LIB_STATE_H
#define CLIPPER_LIB_STATE_H

#include <memory>
#include <unordered_map>


namespace clipper {
    
    class State {
    public:

        ~State() = default;
        State(std::vector<VersionedModelId>& models) = default;
        std::vector<VersionedModelId>& models;
        std::unordered_map<VersionedModelId, std::map<std::string, double>> model_distributions;
    };

    class EpsilonGreedyState: public State {
    public:
        ~EpsilonGreedyState() = default;
        EpsilonGreedyState(std::vector<VersionedModelId> models);
        std::unordered_map<VersionedModelId, std::map<std::string, double>> model_distributions;
    };
}


#endif // CLIPPER_LIB_STATE_H≠++
