//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCSYSTEMINFORMATION_H
#define HAKC_HAKCSYSTEMINFORMATION_H

#include <set>

#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include "HAKCCompartment.h"
#include "HAKCFile.h"
#include "HAKCSymbol.h"
#include "HAKCAllocationSize.h"

using namespace llvm;

namespace hakc {
    class HAKCSystemInformation {
    protected:
        Module &M;
        std::set<std::shared_ptr<HAKCCompartment>> compartments;
        std::set<std::shared_ptr<HAKCSymbol>> symbols;
        StringRef ARCH;
        StringRef PLATFORM;

        bool PathContainsPath(StringRef Path1, StringRef Path2);

        std::set<std::shared_ptr<HAKCCompartment>> getCompartments(hakc_compartment_id_t ID);

        static std::string getColorStringFromValue(ConstantInt *color);

    public:
        std::map<StringRef, std::set<StringRef>> METHODS; 
        std::map<StringRef, HAKCAllocationSize> KernelAllocationSizeMap;

        HAKCSystemInformation(Module &M);

        std::set<std::shared_ptr<HAKCSymbol>> getSymbols(StringRef name);

        std::shared_ptr<HAKCCompartment> getCompartment(hakc_compartment_id_t id, sym_color_t color);

        Module &getModule();

        std::shared_ptr<HAKCSymbol> findSymbol(GlobalValue *GV);

        std::string getCompartmentYamlPath();
        
        std::string getArchYamlPath();
        
        std::string getHAKCPassMode();

        // std::string getCustomYamlPath(std::string str);
        std::string getCustomYamlPath();

        void ProcessCompartmentYaml();

        void ProcessArchYaml();

        void Init();

        hakc_access_token_t getEntryToken(hakc_compartment_id_t CompartmentID);

        bool ContainsCompartmentalizedSymbols(Module &M);

    };

} // hakc

#endif //HAKC_HAKCSYSTEMINFORMATION_H
