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

        HAKCCompartment GetCompartment(GlobalValue *GV);

        HAKCCompartmentDivision GetDivision(GlobalValue *GV);

        HAKCCompartmentDivision
        GetDivision(hakc_compartment_id_t CompartmentID, hakc_compartment_division_t DivisionID);

        HAKC_Division_ID GetDivisionID(GlobalValue *GV);

        HAKCCompartment GetCompartment(hakc_compartment_id_t ID);

        HAKCTypeIdentifier &GetTypeIdentifier();

    protected:
        HAKCYamlCompartmentalizationPolicy YamlPolicy;
        Module &LLVMModule;
        HAKCCompartment KernelCompartment;
        HAKCTypeIdentifier TypeIdentifier;
        std::map<hakc_compartment_id_t, HAKCCompartment> Compartments;
        std::map<GlobalValue *, HAKCCompartmentDivision> GlobalValueMapping;
    };

} // hakc

#endif //HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
