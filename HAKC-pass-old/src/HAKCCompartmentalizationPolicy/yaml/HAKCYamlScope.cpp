//
// Created by de29664 on 7/29/24.
//

#include "HAKCCompartmentalizationPolicy/yaml/HAKCYamlScope.h"

#include <utility>


namespace hakc {
    HAKCYamlScope::HAKCYamlScope(hakc_scope_t Scope, std::string LocalScope) : LocalScope(std::move(LocalScope)),
                                                                               Scope(Scope) {

    }

} // hakc
