//
// Created by 
//

#ifndef HAKC_HAKCYAMLPARSER_H
#define HAKC_HAKCYAMLPARSER_H

#include <set>
#include <vector>
#include <string>
#include <memory>

#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/YAMLParser.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

#include "HAKC-defs.h"
#include "HAKCSymbol.h"
#include "HAKCYaml.h"
// #include "HAKCCompartment.h"
// #include "HAKCFile.h"
// #include "HAKCAnalysis/CommonHAKCAnalysis.h"


using namespace llvm;

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::YamlSymbol)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::YamlFile)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::YamlCompartment)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::YamlClique)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::YamlMethodsInformation)

template<>
struct yaml::ScalarEnumerationTraits<hakc::sym_color_t> {
    static void enumeration(yaml::IO &io, hakc::sym_color_t &value) {
        io.enumCase(value, "SILVER_CLIQUE", hakc::SILVER_CLIQUE);
        io.enumCase(value, "GREEN_CLIQUE", hakc::GREEN_CLIQUE);
        io.enumCase(value, "RED_CLIQUE", hakc::RED_CLIQUE);
        io.enumCase(value, "ORANGE_CLIQUE", hakc::ORANGE_CLIQUE);
        io.enumCase(value, "YELLOW_CLIQUE", hakc::YELLOW_CLIQUE);
        io.enumCase(value, "PURPLE_CLIQUE", hakc::PURPLE_CLIQUE);
        io.enumCase(value, "BLUE_CLIQUE", hakc::BLUE_CLIQUE);
        io.enumCase(value, "GREY_CLIQUE", hakc::GREY_CLIQUE);
        io.enumCase(value, "PINK_CLIQUE", hakc::PINK_CLIQUE);
        io.enumCase(value, "BROWN_CLIQUE", hakc::BROWN_CLIQUE);
        io.enumCase(value, "WHITE_CLIQUE", hakc::WHITE_CLIQUE);
        io.enumCase(value, "BLACK_CLIQUE", hakc::BLACK_CLIQUE);
        io.enumCase(value, "TEAL_CLIQUE", hakc::TEAL_CLIQUE);
        io.enumCase(value, "VIOLET_CLIQUE", hakc::VIOLET_CLIQUE);
        io.enumCase(value, "CRIMSON_CLIQUE", hakc::CRIMSON_CLIQUE);
        io.enumCase(value, "GOLD_CLIQUE", hakc::GOLD_CLIQUE);
        io.enumCase(value, "NO_CLIQUE", hakc::NO_CLIQUE);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlSymbol> {
    static void mapping(yaml::IO &io, hakc::YamlSymbol &info) {
        io.mapRequired("CLIQUE", info.color);
        io.mapRequired("NAME", info.name);
        io.mapRequired("COMPARTMENT", info.compartment);
        io.mapRequired("IS_GLOBAL", info.is_global);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlClique> {
    static void mapping(yaml::IO &io, hakc::YamlClique &info) {
        io.mapRequired("ACCESS_TOKEN", info.access_token);
        io.mapRequired("COLOR", info.color);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlFile> {
    static void mapping(yaml::IO &io, hakc::YamlFile &info) {
        io.mapOptional("GUID", info.guid);
        io.mapRequired("PATH", info.name);
        io.mapOptional("SYMBOLS", info.symbols);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlCompartment> {
    static void mapping(yaml::IO &io, hakc::YamlCompartment &info) {
        io.mapRequired("ID", info.id);
        io.mapOptional("TARGETS", info.targets);
        io.mapRequired("CLIQUES", info.cliques);
        io.mapOptional("ENTRY_TOKEN", info.entry_token);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlInformation> {
    static void mapping(yaml::IO &io, hakc::YamlInformation &info) {
        io.mapRequired("COMPARTMENTS", info.compartments);
        io.mapRequired("FILES", info.files);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlMethodsInformation> {
    static void mapping(yaml::IO &io, hakc::YamlMethodsInformation &info) {
        io.mapRequired("NAME", info.NAME);
        io.mapRequired("FUNCTIONS", info.FUNCTIONS);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlArchInformation> {
    static void mapping(yaml::IO &io, hakc::YamlArchInformation &info) {
        io.mapRequired("ARCH", info.ARCH);
        io.mapRequired("PLATFORM", info.PLATFORM);
        io.mapRequired("METHODS", info.METHODS);
    }
};

template<>
struct yaml::MappingTraits<hakc::YamlHAKCInformation> {
    static void mapping(yaml::IO &io, hakc::YamlHAKCInformation &info) {
        io.mapRequired("SYSTEMINFO", info.SYSTEMINFO);
    }
};

namespace hakc {
    class HAKCYAMLParser {
    public:
        Module &M;
        std::string ARCH;
        std::string PLATFORM;
        std::vector<StringRef> tmp;
        std::map<std::string, std::set<std::string>> *METHODS; 
        std::set<std::shared_ptr<HAKCCompartment>> compartments;
        std::set<std::shared_ptr<HAKCSymbol>> symbols;
        HAKCYAMLParser(Module &M);
        std::map<std::string, std::set<std::string>> *GetMethods();
        void ParseArchYaml();
        void PrintStack();
    };

} // hakc

#endif //HAKC_HAKCYAMLPARSER_H
