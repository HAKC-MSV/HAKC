//
// Created by de29664 on 7/29/24.
//

#include "HAKCCompartmentalizationPolicy/HAKCYamlSymbol.h"

#include "llvm/Support/YAMLTraits.h"

template<>
struct llvm::yaml::MappingTraits<hakc::HAKCYamlType> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlType &Type) {
        hakc::HAKCYamlType::YamlMapping(io, Type);
    }
};

template<>
struct llvm::yaml::MappingTraits<hakc::HAKCYamlScope> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlScope &Scope) {
        hakc::HAKCYamlScope::YamlMapping(io, Scope);
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
    HAKCYamlSymbol::HAKCYamlSymbol() : Type(), Scope(), Name(), Definition(), CompartmentID(0)  {

    }
} // hakc
