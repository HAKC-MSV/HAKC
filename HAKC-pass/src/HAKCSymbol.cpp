//
// Created by de29664 on 3/21/23.
//

#include "HAKCSymbol.h"

#include <utility>

namespace hakc {
    HAKCSymbol::HAKCSymbol(std::string sym_name, std::shared_ptr<HAKCCompartment> compartment,
                           std::shared_ptr<HAKCFile> file, bool is_global)
            : name(std::move(sym_name)), file(std::move(file)), compartment(std::move(compartment)), is_global(is_global) {
    }

    StringRef HAKCSymbol::getName() {
        return name;
    }

    std::shared_ptr<HAKCCompartment> HAKCSymbol::getCompartment() {
        return compartment;
    }

    bool HAKCSymbol::isGlobal() const {
        return is_global;
    }

    std::shared_ptr<HAKCFile> HAKCSymbol::getFile() {
        return file;
    }

    hakc_compartment_id_t HAKCSymbol::getCompartmentID() {
        return getCompartment()->getID();
    }
}
