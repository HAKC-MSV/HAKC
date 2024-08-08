//
// Created by de29664 on 7/29/24.
//

#include "HAKCCompartmentalizationPolicy/yaml/HAKCYamlSymbol.h"

#include "llvm/Support/YAMLTraits.h"

namespace hakc {
    HAKCYamlSymbol::HAKCYamlSymbol() : Type(), Scope(), Name(), Definition(), CompartmentID(0), DivisionID(0) {

    }

    raw_ostream &operator<<(raw_ostream &os, HAKCYamlSymbol &YamlSymbol) {
        os << "HAKCSymbol(Name=" << YamlSymbol.Name << ", DbgType=" << YamlSymbol.Type.DebugType << ", LLVMType=" << YamlSymbol.Type.LLVMType << ", Scope=";
        if(YamlSymbol.Scope.Scope == hakc_global_scope) {
            os << "global";
        } else {
            os << "local, LocalScope=" << YamlSymbol.Scope.LocalScope;
        }
        os << ")";
        return os;
    }


} // hakc
