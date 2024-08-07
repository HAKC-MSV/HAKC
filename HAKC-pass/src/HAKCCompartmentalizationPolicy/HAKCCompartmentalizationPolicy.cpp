//
// Created by de29664 on 7/29/24.
//

#include "HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"
#include "HAKCAnalysis/HAKCModuleAnalysis.h"

#include "llvm/Support/FileSystem.h"

#include "HAKCCompartmentalizationPolicy/yaml/HAKCMappings.h"

namespace hakc {
    HAKCCompartmentalizationPolicy::HAKCCompartmentalizationPolicy(Module &M, HAKCModuleAnalysis *HAKCAnalysis)
            : YamlPolicy(), LLVMContext(M.getContext()),
              KernelCompartment(KERNEL_COMPARTMENT, KERNEL_ACCESS_TOKEN, M.getContext()),
              TypeIdentifier(M, HAKCAnalysis) {

    }

    void HAKCCompartmentalizationPolicy::ReadCompartmentalizationPolicy(const std::string &YamlPath) {
        if (!sys::fs::exists(YamlPath)) {
            CommonHAKCAnalysis::getWriter() << "Could not find YAML file " << YamlPath << "\n";
            throw std::exception();
        } else if (!sys::fs::is_regular_file(YamlPath)) {
            CommonHAKCAnalysis::getWriter() << YamlPath << " is not a regular file\n";
            throw std::exception();
        }

        ErrorOr<std::unique_ptr<MemoryBuffer>> mb = MemoryBuffer::getFile(YamlPath);
        yaml::Input yin(mb.get()->getMemBufferRef().getBuffer());

        if (yin.error()) {
            CommonHAKCAnalysis::getWriter() << "Error parsing " << YamlPath << "\n";
            throw std::exception();
        }
        yin >> YamlPolicy;
    }

    HAKCTypeIdentifier &HAKCCompartmentalizationPolicy::GetTypeIdentifier() {
        return TypeIdentifier;
    }

    HAKCCompartment &HAKCCompartmentalizationPolicy::GetCompartment(GlobalValue *GV) {
        /* TODO: Implement */
        return KernelCompartment;
    }

    HAKC_Division_ID HAKCCompartmentalizationPolicy::GetDivision(GlobalValue *GV) {
        /* TODO: Implement */
        return ConstantInt::get(IntegerType::get(LLVMContext, 64), KERNEL_DIVISION);
    }

    HAKCCompartment &HAKCCompartmentalizationPolicy::GetCompartment(hakc_compartment_id_t ID) {
        /* TODO: Implement */
        return KernelCompartment;
    }

} // hakc
