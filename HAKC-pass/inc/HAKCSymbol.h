//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCSYMBOL_H
#define HAKC_HAKCSYMBOL_H

#include "HAKCPass.h"
#include "HAKCFile.h"
#include "HAKCCompartment.h"

namespace hakc {

    class HAKCFile;

    class HAKCCompartment;

    class HAKCSymbol {
    protected:
        std::string name;
        std::shared_ptr<HAKCFile> file;
        std::shared_ptr<HAKCCompartment> compartment;
        bool is_global;

    public:
        HAKCSymbol(std::string name, std::shared_ptr<HAKCCompartment> compartment, std::shared_ptr<HAKCFile> path, bool
        is_global);

        std::shared_ptr<HAKCCompartment> getCompartment();

        StringRef getName();

        std::shared_ptr<HAKCFile> getFile();

        bool isGlobal() const;

        hakc_compartment_id_t getCompartmentID();

        friend raw_ostream &operator<<(raw_ostream &os, const std::shared_ptr<HAKCSymbol> &HAKCSymbolP);
    };
}


#endif //HAKC_HAKCSYMBOL_H
