//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCFILE_H
#define HAKC_HAKCFILE_H

#include <set>

#include "HAKCCompartment.h"
#include "HAKCYaml.h"

using namespace llvm;

namespace hakc {

    class HAKCSymbol;

    class HAKCCompartment;

    class HAKCFile {
    protected:
        std::string path;
        int64_t guid;

    public:
        HAKCFile(YamlFile &file);

        StringRef GetPath();

        int64_t GetGUID();
    };

} // hakc

#endif //HAKC_HAKCFILE_H
