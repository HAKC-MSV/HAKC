//
// Created by de29664 on 8/7/24.
//

#ifndef HAKC_HAKCMAPPINGS_H
#define HAKC_HAKCMAPPINGS_H

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYamlCompartment)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYamlFile)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYamlClique)
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYamlSymbol)

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
        io.mapRequired("name", Clique.DivisionID);
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
        io.mapRequired("division_id", Symbol.DivisionID);
        io.mapRequired("definition", Symbol.Definition);
        io.mapRequired("name", Symbol.Name);
        io.mapRequired("scope", Symbol.Scope);
        io.mapRequired("type", Symbol.Type);
    }
};

#endif //HAKC_HAKCMAPPINGS_H
