//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCYAML_H
#define HAKC_HAKCYAML_H

#include <string>
#include <vector>

// #include "HAKC-defs.h"

namespace hakc {

    // Structs for the yaml template
    struct YamlClique {
        int64_t access_token;
        sym_color_t color;
    };

    struct YamlCompartment {
        int64_t id;
        int64_t entry_token;
        std::vector<int64_t> targets;
        std::vector<YamlClique> cliques;
    };

    struct YamlSymbol {
        int64_t compartment;
        sym_color_t color;
        std::string name;
        bool is_global;
    };

    struct YamlFile {
        int64_t guid;
        std::string name;
        std::vector<YamlSymbol> symbols;
    };

    struct YamlInformation {
        std::vector<YamlCompartment> compartments;
        std::vector<YamlFile> files;
    };

    struct YamlMethodsInformation {
        std::string NAME;
        std::vector<std::string> FUNCTIONS;
    };

    struct YamlArchInformation {
        std::string ARCH;
        std::string PLATFORM;
        std::vector<YamlMethodsInformation> METHODS;
    };

    struct YamlHAKCInformation {
        YamlArchInformation SYSTEMINFO;
    };
}// namespace hakc

#endif//HAKC_HAKCYAML_H
