//
// Created by de29664 on 7/29/24.
//

#include "HAKCCompartmentalizationPolicy/HAKCYamlScope.h"

#include <utility>

template<>
struct yaml::ScalarEnumerationTraits<hakc::hakc_scope_t> {
    static void enumeration(yaml::IO &io, hakc::hakc_scope_t &value) {
        io.enumCase(value, "global", hakc::hakc_global_scope);
        io.enumCase(value, "local", hakc::hakc_local_scope);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYamlScope> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlScope &Scope) {
        hakc::HAKCYamlScope::YamlMapping(io, Scope);
    }
};

namespace hakc {
    HAKCYamlScope::HAKCYamlScope(hakc_scope_t Scope, std::string LocalScope) : LocalScope(std::move(LocalScope)),
                                                                               Scope(Scope) {

    }

    void HAKCYamlScope::YamlMapping(yaml::IO &io, HAKCYamlScope &Scope) {
        io.mapOptional("local_scope_name", Scope.LocalScope, "");
        io.mapRequired("scope", Scope.Scope);
    }
} // hakc
