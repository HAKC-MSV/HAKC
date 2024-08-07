//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
#define HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H

#include "HAKCCompartmentalizationPolicy/yaml/HAKCYamlCompartmentalizationPolicy.h"
#include "HAKCCompartment.h"
#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"

namespace hakc {

    class HAKCModuleAnalysis;
    class HAKCTypeIdentifier;

    class HAKCCompartmentalizationPolicy {
    public:
        explicit HAKCCompartmentalizationPolicy(Module &M, HAKCModuleAnalysis *HAKCAnalysis);

        void ReadCompartmentalizationPolicy(const std::string &YamlPath);

        HAKCCompartment &GetCompartment(GlobalValue *GV);

        HAKC_Division_ID GetDivision(GlobalValue *GV);

        HAKCCompartment &GetCompartment(hakc_compartment_id_t ID);

        HAKCTypeIdentifier& GetTypeIdentifier();

    protected:
        HAKCYamlCompartmentalizationPolicy YamlPolicy;
        LLVMContext &LLVMContext;
        HAKCCompartment KernelCompartment;
        HAKCTypeIdentifier TypeIdentifier;
    };

} // hakc

#endif //HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
