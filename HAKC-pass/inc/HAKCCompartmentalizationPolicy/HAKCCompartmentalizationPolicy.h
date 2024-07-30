//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
#define HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H

#include "HAKCCompartmentalizationPolicy/yaml/HAKCYamlCompartmentalizationPolicy.h"
#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"

namespace hakc {

    class HAKCCompartmentalizationPolicy {
    public:
        explicit HAKCCompartmentalizationPolicy(HAKCTypeIdentifier &TypeIdentifier);

        void ReadCompartmentalizationPolicy(std::string YamlPath);
        ConstantInt* GetCompartment(GlobalValue *GV);

    protected:
        HAKCYamlCompartmentalizationPolicy YamlPolicy;
        HAKCTypeIdentifier &TypeIdentifier;
    };

} // hakc

#endif //HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
