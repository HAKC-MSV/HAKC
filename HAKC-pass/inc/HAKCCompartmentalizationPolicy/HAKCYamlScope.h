//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCYAMLSCOPE_H
#define HAKC_HAKCYAMLSCOPE_H

#include <string>
#include "HAKC-defs.h"

#include "llvm/Support/YAMLTraits.h"

namespace hakc {

    class HAKCYamlScope {
    public:
        HAKCYamlScope(hakc_scope_t Scope, std::string LocalScope);
        HAKCYamlScope() = default;

        std::string LocalScope;
        hakc_scope_t Scope;

        static void YamlMapping(yaml::IO &io, hakc::HAKCYamlScope &Scope);
    };

} // hakc

#endif //HAKC_HAKCYAMLSCOPE_H
