//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCINFO_H
#define HAKC_HAKCINFO_H

#include "llvm/IR/DerivedTypes.h"

#include "HAKCTypeIdentifier.h"

#include <set>

using namespace llvm;

namespace hakc {

    class HAKCInfo {
    public:
        HAKCInfo(HAKCTypeIdentifier &identifier, StringRef directory, StringRef file, unsigned line);

        StringRef getDefinitionDirectory();

        unsigned getDefinitionLine();

        StringRef getDefinitionFile();

        virtual std::string getTypeStringRepresentation() = 0;

        virtual std::string getYaml();

        virtual std::string getName() = 0;

        virtual std::string getHash();

    protected:
        HAKCTypeIdentifier &identifier;
        StringRef directory;
        StringRef file;
        unsigned line;
    };

} // hakc

#endif //HAKC_HAKCINFO_H
