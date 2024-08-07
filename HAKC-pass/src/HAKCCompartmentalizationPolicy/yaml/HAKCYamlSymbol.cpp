//
// Created by de29664 on 7/29/24.
//

#include "HAKCCompartmentalizationPolicy/yaml/HAKCYamlSymbol.h"

#include "llvm/Support/YAMLTraits.h"

namespace hakc {
    HAKCYamlSymbol::HAKCYamlSymbol() : Type(), Scope(), Name(), Definition(), CompartmentID(0), DivisionID(0) {

    }
} // hakc
