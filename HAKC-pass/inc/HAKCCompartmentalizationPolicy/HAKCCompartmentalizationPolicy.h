//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
#define HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H

#include "HAKCCompartmentalizationPolicy/yaml/HAKCYamlCompartmentalizationPolicy.h"
#include "HAKCCompartment.h"


namespace hakc {

    class HAKCCompartmentalizationPolicy {
    public:
        explicit HAKCCompartmentalizationPolicy(class LLVMContext &LLVMContext);

        void ReadCompartmentalizationPolicy(const std::string& YamlPath);
        HAKCCompartment& GetCompartment(GlobalValue *GV);
        HAKC_Division_ID GetDivision(GlobalValue *GV);

    protected:
        HAKCYamlCompartmentalizationPolicy YamlPolicy;
        LLVMContext &LLVMContext;
        HAKCCompartment KernelCompartment;
    };

} // hakc

#endif //HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
