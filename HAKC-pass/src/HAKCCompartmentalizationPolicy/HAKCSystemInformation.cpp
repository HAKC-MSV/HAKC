//
// Created by de29664 on 3/21/23.
//

#include "HAKCCompartmentalizationPolicy/HAKCSystemInformation.h"
#include "HAKCCompartment.h"
#include "HAKCFile.h"
#include "HAKCSymbol.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"

#include <memory>

#include "llvm/Support/YAMLParser.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

namespace hakc {

    HAKCSystemInformation::HAKCSystemInformation(Module &M) : M(M) {
        // todo: maybe fix weird memory issue here
        parser = std::make_shared<HAKCYAMLParser>(M);
        ARCH = parser->ARCH;
        PLATFORM = parser->PLATFORM;
        ProcessYAML();
        ProcessCompartmentYaml();
    }

    std::map<std::string, std::set<std::string>> *HAKCSystemInformation::GetMethods(){
        return parser->GetMethods();
    }

    Module &HAKCSystemInformation::getModule() {
        return M;
    }

    void HAKCSystemInformation::ProcessYAML() {
        std::set<std::string> KernelAllocationSizeMapStrings = (*GetMethods())["GetKernelAllocationSizeMap"];
        for (std::set<std::string>::iterator ptr = KernelAllocationSizeMapStrings.begin(); ptr != KernelAllocationSizeMapStrings.end(); ptr++) {
            CommonHAKCAnalysis::getWriter() << "\t in kernel alloc " << *ptr << "\n";
            HAKCAllocationSize Size(*ptr);
            KernelAllocationSizeMap.insert({*ptr, Size});
        }

        DebugInfo.processModule(M);
        DetectCompartmentalization();
    }

    bool HAKCSystemInformation::ContainsCompartmentalizedSymbols(Module &M) {
        for (auto Symbol : symbols) {
            if (CommonHAKCAnalysis::IsKernelCompartment(Symbol->getCompartmentID())) {
                continue;
            }
            COMPARTMENT_CHECK(&GV);
        }
        for (auto &F: M.functions()) {
            if (F.isIntrinsic() || F.isDeclaration()) {
                continue;
            }
            COMPARTMENT_CHECK(&F);
        }
    }

    bool HAKCSystemInformation::ContainsCompartmentalizedSymbols() const {
        return ModuleContainsCompartmentalizedSymbols;
    }

    bool HAKCSystemInformation::SymbolIsInScope(const std::shared_ptr<HAKCSymbol> &Symbol, const DIScope *Scope) {
        std::string ScopeFile;
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Checking if " << Symbol << " is in Scope with " << *Scope << "\n";
        }

        if (sys::path::is_relative(Scope->getFilename())) {
            ScopeFile = Scope->getDirectory().str();
            if (!Scope->getDirectory().endswith(llvm::sys::path::get_separator())) {
                ScopeFile += llvm::sys::path::get_separator();
            }
            ScopeFile += Scope->getFilename();
        } else {
            ScopeFile = Scope->getFilename().str();
        }

        std::error_code err;
        SmallString<256> ScopePath;
        err = sys::fs::real_path(ScopeFile, ScopePath, true);
        if (err) {
            CommonHAKCAnalysis::getWriter() << "Could not get real path to " << ScopeFile << "\n";
            throw std::exception();
        }

        auto TransformedScopePath = HAKCTypeIdentifier::GetTransformedPath(ScopePath);
        return TransformedScopePath == Symbol->getFile()->GetPath();
    }

    std::shared_ptr<HAKCSymbol> HAKCSystemInformation::findSymbol(GlobalValue *GV) {
        StringRef Name = GV->getName();
        if (auto *Func = dyn_cast<Function>(GV)) {
            Name = CommonHAKCAnalysis::GetFunctionName(Func);
        }
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Getting Symbol named " << Name << "\n";
        }
        auto Symbols = getSymbols(Name);
        if (Symbols.empty()) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "No symbols found\n";
            }
            return nullptr;
        } else if (Symbols.size() == 1) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Found one symbol " << *Symbols.begin() << "\n";
            }
            return *Symbols.begin();
        }
        for (auto Symbol: Symbols) {
            if (isa<Function>(GV)) {
                for (auto *DIF: DebugInfo.subprograms()) {
                    if (SymbolIsInScope(Symbol, DIF->getScope())) {
                        return Symbol;
                    }
                }
            } else {
                for (auto *DIGV: DebugInfo.global_variables()) {
                    if (SymbolIsInScope(Symbol, DIGV->getVariable()->getScope())) {
                        return Symbol;
                    }
                }
            }
        }
        return *Symbols.begin();
    }

    std::set<std::shared_ptr<HAKCSymbol>> HAKCSystemInformation::getSymbols(StringRef name) {
        std::set<std::shared_ptr<HAKCSymbol>> result;
        for (auto &Symbol : symbols) {
            if (Symbol->getName() == name) {
                result.insert(Symbol);
            }
        }
        return result;
    }

    void HAKCSystemInformation::SetDebugActive(bool ActiveDebug) {
        this->DebugActive = ActiveDebug;
    }

    std::string HAKCSystemInformation::getColorStringFromValue(ConstantInt *color) {
        switch (color->getZExtValue()) {
            case SILVER_CLIQUE:
                return "SILVER_CLIQUE";
            case GREEN_CLIQUE:
                return "GREEN_CLIQUE";
            case RED_CLIQUE:
                return "RED_CLIQUE";
            case ORANGE_CLIQUE:
                return "ORANGE_CLIQUE";
            case YELLOW_CLIQUE:
                return "YELLOW_CLIQUE";
            case PURPLE_CLIQUE:
                return "PURPLE_CLIQUE";
            case BLUE_CLIQUE:
                return "BLUE_CLIQUE";
            case GREY_CLIQUE:
                return "GREY_CLIQUE";
            case PINK_CLIQUE:
                return "PINK_CLIQUE";
            case BROWN_CLIQUE:
                return "BROWN_CLIQUE";
            case WHITE_CLIQUE:
                return "WHITE_CLIQUE";
            case BLACK_CLIQUE:
                return "BLACK_CLIQUE";
            case TEAL_CLIQUE:
                return "TEAL_CLIQUE";
            case VIOLET_CLIQUE:
                return "VIOLET_CLIQUE";
            case CRIMSON_CLIQUE:
                return "CRIMSON_CLIQUE";
            case GOLD_CLIQUE:
                return "GOLD_CLIQUE";
            case NO_CLIQUE:
                return "NO_CLIQUE";
            default:
                CommonHAKCAnalysis::getWriter() << "number " << color->getZExtValue() << "isn't a valid color\n";
                return "INVALID_CLIQUE";
        }
    }
}// namespace hakc
