//
// Created by de29664 on 5/2/23.
//

#include "HAKCTypeIdentifier/HAKCGlobalInfo.h"
#include "HAKCTypeIdentifier/HAKCTypeInfo.h"


#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"


#include <sstream>

namespace hakc {
    HAKCGlobalInfo::HAKCGlobalInfo(GlobalObject *GO, HAKCTypeIdentifier &identifier, StringRef directory,
                                   StringRef file, unsigned int line) :
            HAKCInfo(identifier, directory, file, line), GO(GO), escapingSymbols(), usedInIndirectCalls(false),
            usedInUnknownOriginStore(false) {
        if (directory.empty() && GO->hasExactDefinition()) {
            auto *c_dir = std::getenv("PWD");
            if (c_dir) {
                this->directory = c_dir;
            }
        }

        if (file.empty() && GO->hasExactDefinition()) {
            this->file = GO->getParent()->getSourceFileName();
        }
    }

    std::string HAKCGlobalInfo::getName() {
        StringRef Name = GO->getName();
        if (auto *Func = dyn_cast<Function>(GO)) {
            Name = CommonHAKCAnalysis::GetFunctionName(Func);
        }
        return Name.str();
    }

    void HAKCGlobalInfo::addEscapingSymbol(std::string escapingSymbol) {
        escapingSymbols.insert(escapingSymbol);
    }

    bool HAKCGlobalInfo::isDefinedInCU() {
        bool isDefinedInCU = GO->hasExactDefinition();
        return isDefinedInCU;
    }

    GlobalObject *HAKCGlobalInfo::getValue() {
        return GO;
    }

    std::string HAKCGlobalInfo::getTypeStringRepresentation() {
        if (auto *F = dyn_cast<Function>(GO)) {
            return HAKCTypeInfo::getTypeString(F->getFunctionType()->getPointerTo());
        } else {
            return HAKCTypeInfo::getTypeString(GO->getType()->getPointerElementType());
        }
    }

    std::string HAKCGlobalInfo::getYaml() {
        std::stringstream out;

        out << HAKCInfo::getYaml();
        out << "  is-global: ";
        if (isa<GlobalVariable>(GO)) {
            out << "y\n";
        } else {
            out << "n\n";
        }
        out << "  is-defined: ";
        if (isDefinedInCU()) {
            out << "y\n";
        } else {
            out << "n\n";
        }
        out << "  valid-decl-linkage: ";
        if (GO->hasValidDeclarationLinkage()) {
            out << "y\n";
        } else {
            out << "n\n";
        }
        if (!escapingSymbols.empty()) {
            out << "  escapes-to:\n";
            for (auto &escapingSymbol: escapingSymbols) {
                out << "    - " << escapingSymbol << "\n";
            }
        }
        return out.str();
    }

    void HAKCGlobalInfo::setUsedInIndirectCalls() {
        usedInIndirectCalls = true;
    }

    void HAKCGlobalInfo::setUsedInUnknownOriginStore() {
        usedInUnknownOriginStore = true;
    }
} // hakc