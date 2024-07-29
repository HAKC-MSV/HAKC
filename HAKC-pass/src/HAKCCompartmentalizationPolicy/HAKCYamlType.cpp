//
// Created by de29664 on 7/29/24.
//

#include "HAKCCompartmentalizationPolicy/HAKCYamlType.h"

template<>
struct llvm::yaml::MappingTraits<hakc::HAKCYamlType> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlType &Type) {
        hakc::HAKCYamlType::YamlMapping(io, Type);
    }
};

namespace hakc {
    void HAKCYamlType::YamlMapping(yaml::IO &io, HAKCYamlType &Type) {
        io.mapOptional("debug_type", Type.DebugType);
        io.mapOptional("llvm_type", Type.LLVMType);
    }
} // hakc
