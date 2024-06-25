//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCINFO_H
#define HAKC_HAKCINFO_H

#include <set>

#include "llvm/ADT/StringRef.h"

using namespace llvm;

namespace hakc {

    class HAKCInfo {
    public:
        virtual std::string GetYaml(unsigned Indents) = 0;

        virtual StringRef GetYamlIdentifier() const = 0;

        virtual std::string GetYamlHeader(unsigned Indents) const;

        virtual StringRef GetName() const;

        friend raw_ostream &operator<<(raw_ostream &os, HAKCInfo &Info);

        static unsigned int IndentSpaces();

    protected:
        bool DebugActive;
        std::string Name;

        explicit HAKCInfo(StringRef Name, bool DebugActive);

    };

} // hakc

#endif //HAKC_HAKCINFO_H
