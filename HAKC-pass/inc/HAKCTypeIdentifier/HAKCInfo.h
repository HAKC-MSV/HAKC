//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCINFO_H
#define HAKC_HAKCINFO_H

#include "llvm/IR/DerivedTypes.h"

#include <set>

using namespace llvm;

namespace hakc {

    class HAKCTypeIdentifier;

    class HAKCInfo {
    public:
        HAKCInfo(HAKCTypeIdentifier &identifier, StringRef directory, StringRef file, unsigned line);

        StringRef getDefinitionDirectory();

        unsigned getDefinitionLine();

        StringRef getDefinitionFile();

        std::string getTypeStringRepresentation() ;

        std::string getYaml();

        std::string getName() ;

        std::string getHash();

    protected:
        HAKCTypeIdentifier &identifier;
        StringRef directory;
        StringRef file;
        unsigned line;
    };

} // hakc

#endif //HAKC_HAKCINFO_H
