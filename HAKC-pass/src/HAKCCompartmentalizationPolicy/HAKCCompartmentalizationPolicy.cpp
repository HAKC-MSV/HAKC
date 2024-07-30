//
// Created by de29664 on 7/29/24.
//

#include "HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"

#include "llvm/Support/FileSystem.h"

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYamlCompartment)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYamlFile)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYamlClique)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYamlSymbol)

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
struct yaml::ScalarEnumerationTraits<hakc::hakc_scope_t> {
    static void enumeration(yaml::IO &io, hakc::hakc_scope_t &value) {
        io.enumCase(value, "global", hakc::hakc_global_scope);
        io.enumCase(value, "local", hakc::hakc_local_scope);
    }
};

template<>
struct llvm::yaml::MappingTraits<hakc::HAKCYamlType> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlType &Type) {
        io.mapOptional("debug_type", Type.DebugType);
        io.mapOptional("llvm_type", Type.LLVMType);
    }
};

template<>
struct llvm::yaml::MappingTraits<hakc::HAKCYamlScope> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlScope &Scope) {
        io.mapOptional("local_scope_name", Scope.LocalScope, "");
        io.mapRequired("scope", Scope.Scope);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYamlClique> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlClique &Clique) {
        io.mapRequired("access_token", Clique.AccessToken);
        io.mapRequired("name", Clique.Color);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYamlCompartment> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlCompartment &Compartment) {
        io.mapRequired("cliques", Compartment.Cliques);
        io.mapRequired("compartment_id", Compartment.CompartmentID);
        io.mapRequired("targets", Compartment.Targets);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYamlFile> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlFile &File) {
        io.mapRequired("file", File.Filename);
        io.mapRequired("symbol", File.Symbols);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYamlCompartmentalizationPolicy> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlCompartmentalizationPolicy &YamlPolicy) {
        io.mapRequired("COMPARTMENTS", YamlPolicy.Compartments);
        io.mapRequired("FILES", YamlPolicy.Files);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYamlSymbol> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlSymbol &Symbol) {
        io.mapRequired("compartment_id", Symbol.CompartmentID);
        io.mapRequired("definition", Symbol.Definition);
        io.mapRequired("name", Symbol.Name);
        io.mapRequired("scope", Symbol.Scope);
        io.mapRequired("type", Symbol.Type);
    }
};

namespace hakc {
    HAKCCompartmentalizationPolicy::HAKCCompartmentalizationPolicy(HAKCTypeIdentifier &TypeIdentifier)
            : YamlPolicy(), TypeIdentifier(TypeIdentifier) {

    }

    void HAKCCompartmentalizationPolicy::ReadCompartmentalizationPolicy(std::string YamlPath) {
        if (!sys::fs::exists(YamlPath)) {
            CommonHAKCAnalysis::getWriter() << "Could not find YAML file " << YamlPath << "\n";
            throw std::exception();
        } else if (!sys::fs::is_regular_file(YamlPath)) {
            CommonHAKCAnalysis::getWriter() << YamlPath << " is not a regular file\n";
            throw std::exception();
        }

        ErrorOr<std::unique_ptr<MemoryBuffer>> mb = MemoryBuffer::getFile(YamlPath);
        yaml::Input yin(mb.get()->getMemBufferRef().getBuffer());

        if (yin.error()) {
            CommonHAKCAnalysis::getWriter() << "Error parsing " << YamlPath << "\n";
            throw std::exception();
        }
        yin >> YamlPolicy;
    }

    ConstantInt *HAKCCompartmentalizationPolicy::GetCompartment(GlobalValue *GV) {
        return nullptr;
    }
} // hakc
