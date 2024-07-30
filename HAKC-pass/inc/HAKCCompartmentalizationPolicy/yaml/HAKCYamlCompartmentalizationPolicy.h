//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCYAMLCOMPARTMENTALIZATIONPOLICY_H
#define HAKC_HAKCYAMLCOMPARTMENTALIZATIONPOLICY_H

#include <vector>
#include "HAKCYamlCompartment.h"
#include "HAKCYamlFile.h"

namespace hakc {

    class HAKCYamlCompartmentalizationPolicy {
    public:
        HAKCYamlCompartmentalizationPolicy() = default;

        std::vector<HAKCYamlCompartment> Compartments;
        std::vector<HAKCYamlFile> Files;
    };

} // hakc

#endif //HAKC_HAKCYAMLCOMPARTMENTALIZATIONPOLICY_H
