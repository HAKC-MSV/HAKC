//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCSYSTEMINFORMATION_H
#define HAKC_HAKCSYSTEMINFORMATION_H

#include <set>

#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"

#include "HAKCCompartment.h"
#include "HAKCFile.h"
#include "HAKCSymbol.h"

using namespace llvm;

namespace hakc {

    class HAKCSystemInformation {
    protected:
        Module &M;
        std::set<std::shared_ptr<HAKCCompartment>> compartments;
        std::set<std::shared_ptr<HAKCSymbol>> symbols;
        bool ModuleContainsCompartmentalizedSymbols;
        DebugInfoFinder DebugInfo;

        std::set<std::shared_ptr<HAKCCompartment>> getCompartments(hakc_compartment_id_t ID);

        static std::string getColorStringFromValue(ConstantInt *color);

        void DetectCompartmentalization();

        bool SymbolIsInScope(std::shared_ptr<HAKCSymbol> Symbol, const DIScope *Scope);

    public:
        HAKCSystemInformation(Module &M);

        std::set<std::shared_ptr<HAKCSymbol>> getSymbols(StringRef name);

        std::shared_ptr<HAKCCompartment> getCompartment(hakc_compartment_id_t id, sym_color_t color);

        std::shared_ptr<HAKCSymbol> findSymbol(GlobalValue *GV);

        static std::string getCompartmentYamlPath();

        hakc_access_token_t getEntryToken(hakc_compartment_id_t CompartmentID);

        bool ContainsCompartmentalizedSymbols();

    };

} // hakc

#endif //HAKC_HAKCSYSTEMINFORMATION_H
