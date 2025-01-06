//
// Created by de29664 on 6/23/23.
//

#include "HAKCFunctionDefinition/HAKCFunctionDefinition.h"

namespace hakc {

    HAKCFunctionDefinition::HAKCFunctionDefinition(Function *F) : F(F) {

    }

    StringRef HAKCFunctionDefinition::GetName() const {
        return F->getName();
    }

    Function *HAKCFunctionDefinition::GetFunction() const {
        return F;
    }
} // hakc
