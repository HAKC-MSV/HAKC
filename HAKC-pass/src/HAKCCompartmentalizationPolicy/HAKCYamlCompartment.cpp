//
// Created by de29664 on 7/29/24.
//

#include "HAKCCompartmentalizationPolicy/HAKCYamlCompartment.h"

#include "llvm/Support/YAMLTraits.h"

#include <utility>

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYamlClique)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYamlCompartment)

template<>
struct yaml::MappingTraits<hakc::HAKCYamlClique> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlClique &Clique) {
        hakc::HAKCYamlClique::YamlMapping(io, Clique);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYamlCompartment> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlCompartment &Compartment) {
        io.mapRequired("cliques", Compartment.Cliques);
        io.mapRequired("compartment_id", Compartment.CompartmentID);
        io.mapRequired("targets", Compartment.Targets);
    }
};

namespace hakc {
    HAKCYamlCompartment::HAKCYamlCompartment(hakc_compartment_id_t CompartmentID, std::vector<HAKCYamlClique> Cliques,
                                             std::vector<hakc_compartment_id_t> Targets) : Cliques(std::move(Cliques)),
                                                                                        CompartmentID(CompartmentID),
                                                                                        Targets(std::move(Targets)) {

    }
} // hakc
