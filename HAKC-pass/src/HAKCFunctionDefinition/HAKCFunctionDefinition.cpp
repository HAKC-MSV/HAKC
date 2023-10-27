//
// Created by de29664 on 6/23/23.
//

#include "HAKCFunctionDefinition/HAKCFunctionDefinition.h"

namespace hakc {

    HAKCFunctionDefinition::HAKCFunctionDefinition(StringRef Name) :
            Name(Name) {

    }

    StringRef HAKCFunctionDefinition::GetName() const {
        return Name;
    }
} // hakc