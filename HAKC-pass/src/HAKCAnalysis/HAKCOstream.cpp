//
// Created by de29664 on 10/28/24.
//

#include "HAKCAnalysis/HAKCOstream.h"

#include "llvm/BinaryFormat/Dwarf.h"

namespace hakc {
    HAKCOstream::HAKCOstream() : os(errs()) {

    }

    raw_ostream &HAKCOstream::GetOS() {
        return os;
    }

    void HAKCOstream::printDIType(const DIType *type, unsigned int indents) {
        if (!type) {
            return;
        }
        for (unsigned i = 0; i < indents; i++) {
            os << "\t";
        }
        os << *type << "\n";
        if (auto *diDerivedType = dyn_cast<DIDerivedType>(type)) {
            if (diDerivedType->getBaseType()) {
                os << "\n";
                printDIType(diDerivedType->getBaseType(), indents + 1);
            }
        } else if (auto *diCompositeType = dyn_cast<DICompositeType>(type)) {
            if (diCompositeType->getTag() == dwarf::DW_TAG_enumeration_type ||
                diCompositeType->getTag() == dwarf::DW_TAG_array_type) {
                os << "\n";
                printDIType(diCompositeType->getBaseType(), indents + 1);
            }
        }
    }
} // hakc
