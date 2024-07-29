//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCYAMLCOMPARTMENT_H
#define HAKC_HAKCYAMLCOMPARTMENT_H

#include "HAKC-defs.h"
#include "HAKCYamlClique.h"

namespace hakc {

    class HAKCYamlCompartment {
    public:
        HAKCYamlCompartment(hakc_compartment_id_t CompartmentID, std::vector<HAKCYamlClique> Cliques,
                            std::vector<hakc_compartment_id_t> Targets);
        HAKCYamlCompartment() = default;

        std::vector<HAKCYamlClique> Cliques;
        hakc_compartment_id_t CompartmentID;
        std::vector<hakc_compartment_id_t> Targets;
    };

} // hakc

#endif //HAKC_HAKCYAMLCOMPARTMENT_H
