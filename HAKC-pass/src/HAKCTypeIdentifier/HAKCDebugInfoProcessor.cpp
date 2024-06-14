//
// Created by de29664 on 6/21/23.
//

#include "HAKCTypeIdentifier/HAKCDebugInfoProcessor.h"
#include "llvm/Transforms/Utils/Local.h"

#include "HAKC-defs.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"

namespace hakc {
    HAKCDebugInfoProcessor::HAKCDebugInfoProcessor(Module &M) :
            M(M), debug(false), infoFinder() {
        infoFinder.processModule(M);
    }

    const StructLayout *
    hakc::HAKCDebugInfoProcessor::getStructLayout(StructType *structType) {
        return M.getDataLayout().getStructLayout(structType);
    }

    bool hakc::HAKCDebugInfoProcessor::functionShouldBeSkipped(Function *F) {
        return (F->isIntrinsic() ||
                CommonHAKCAnalysis::isOutsideTransferFunc(F));
    }

    const DIType *HAKCDebugInfoProcessor::findDiType(StructType *StructTy) {
        if (StructTy->isLiteral()) {
            return nullptr;
        }
        StringRef StructTyName = StructTy->getName().substr(StructTy->getName().find_first_of('.') + 1);
        for (auto *DiType: infoFinder.types()) {
            if (DiType->getName() == StructTyName && DiType->getTag() != dwarf::DW_TAG_member) {
                return DiType;
            }
        }

        return nullptr;
    }

    std::string HAKCDebugInfoProcessor::getStructNamePrefix(const DIType *diType) {
        std::string typeName = "";
        if (diType->getTag() == dwarf::DW_TAG_structure_type) {
            typeName = "struct.";
        } else if (diType->getTag() == dwarf::DW_TAG_class_type) {
            typeName = "class.";
        } else if (diType->getTag() == dwarf::DW_TAG_union_type) {
            typeName = "union.";
        }

        return typeName;
    }

    StructType *HAKCDebugInfoProcessor::getStructType(const DICompositeType *diCompositeType) {
        std::string typeName = getStructNamePrefix(diCompositeType);
        typeName += diCompositeType->getName();
        StructType *structType = StructType::getTypeByName(M.getContext(), typeName);
        return structType;
    }

    uint64_t HAKCDebugInfoProcessor::getDITypeSizeInBits(const DIType *diType) {
        uint64_t size = 0;
        if (auto *diDerivedType = dyn_cast<DIDerivedType>(diType)) {
            if (diDerivedType->getTag() == dwarf::DW_TAG_pointer_type) {
                size = diDerivedType->getSizeInBits();
            } else if (diDerivedType->getTag() != dwarf::DW_TAG_member) {
                size = getDITypeSizeInBits(diDerivedType->getBaseType());
            }
        } else if (auto *diCompositeType = dyn_cast<DICompositeType>(diType)) {
            size = diCompositeType->getSizeInBits();
            if (size <= 0) {
                StructType *structType = getStructType(diCompositeType);
                if (structType) {
                    size = structType->getScalarSizeInBits();
                }
            }
        } else if (auto *diBasicType = dyn_cast<DIBasicType>(diType)) {
            size = diBasicType->getSizeInBits();
        }
        return size;
    }

    bool HAKCDebugInfoProcessor::derivedTypeIsPointer(const DIType *diType) {
        if (diType == nullptr || diType->getTag() == dwarf::DW_TAG_pointer_type) {
            return true;
        }
        if (auto *diDerivedType = dyn_cast<DIDerivedType>(diType)) {
            return derivedTypeIsPointer(diDerivedType->getBaseType());
        } else if (diType->getTag() == dwarf::DW_TAG_array_type) {
            auto *diCompositeType = dyn_cast<DICompositeType>(diType);
            return derivedTypeIsPointer(diCompositeType->getBaseType());
        }
        return false;
    }

    const DIType *HAKCDebugInfoProcessor::unwrapDIType(const DIType *diType) {
        if (auto *diCompositeType = dyn_cast<DICompositeType>(diType)) {
            const DIType *returnDIType = nullptr;
            switch (diType->getTag()) {
                default:
                    returnDIType = diType;
                    break;
                case dwarf::DW_TAG_enumeration_type:
                    if (diCompositeType->getBaseType()) {
                        returnDIType = unwrapDIType(diCompositeType->getBaseType());
                    } else if (diCompositeType->isForwardDecl()) {
                        returnDIType = diType;
                    }
                    break;
            }
            if (returnDIType == nullptr) {
                CommonHAKCAnalysis::getWriter() << "returnDIType is null for ";
                printDIType(diType, 0);
                CommonHAKCAnalysis::getWriter() << "\n";
                throw std::exception();
            }
            return returnDIType;
        }
        if (isa<DIBasicType>(diType) || isa<DISubroutineType>(diType)) {
            return diType;
        } else if (auto *diDerivedType = dyn_cast<DIDerivedType>(diType)) {
            if (diDerivedType->getBaseType() == nullptr) {
                return diType;
            }
            return unwrapDIType(diDerivedType->getBaseType());
        }
        CommonHAKCAnalysis::getWriter() << "Unhandled DIType:\n";
        printDIType(diType, 0);
        CommonHAKCAnalysis::getWriter() << "\n";
        throw std::exception();
    }

    void HAKCDebugInfoProcessor::printDIType(const DIType *type, unsigned int indents) {
        if (!type) {
            return;
        }
        for (unsigned i = 0; i < indents; i++) {
            CommonHAKCAnalysis::getWriter() << "\t";
        }
        type->print(CommonHAKCAnalysis::getWriter());
        if (auto *diDerivedType = dyn_cast<DIDerivedType>(type)) {
            if (diDerivedType->getBaseType()) {
                CommonHAKCAnalysis::getWriter() << "\n";
                printDIType(diDerivedType->getBaseType(), indents + 1);
            }
        } else if (auto *diCompositeType = dyn_cast<DICompositeType>(type)) {
            if (diCompositeType->getTag() == dwarf::DW_TAG_enumeration_type ||
                diCompositeType->getTag() == dwarf::DW_TAG_array_type) {
                CommonHAKCAnalysis::getWriter() << "\n";
                printDIType(diCompositeType->getBaseType(), indents + 1);
            }
        }
    }

    bool HAKCDebugInfoProcessor::debugActive() {
        return debug;
    }

    const DIType *HAKCDebugInfoProcessor::getBaseDefinition(const MDNode *metadata) {
        if (!metadata) {
            CommonHAKCAnalysis::getWriter() << "Null metadata!\n";
            throw std::exception();
        }

        if (auto *gve =
                dyn_cast<DIGlobalVariableExpression>(metadata)) {
            return getBaseDefinition(gve->getVariable());
        } else if (auto *gv =
                dyn_cast<DIGlobalVariable>(metadata)) {
            return gv->getType();
        } else if (auto *type = dyn_cast<DIType>(metadata)) {
            return type;
        } else if (auto *localVariable =
                dyn_cast<DILocalVariable>(metadata)) {
            return localVariable->getType();
        }

        return nullptr;
    }

    DIType *HAKCDebugInfoProcessor::getValueMetadataDIType(Value *v) {
        SmallVector<DbgValueInst *> dbg;
        if (!v) {
            return nullptr;
        }
        findDbgValues(dbg, v);
        for (DbgValueInst *d: dbg) {
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Debug value instruction: ";
                d->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            if (DILocalVariable *localVar = d->getVariable()) {
                return localVar->getType();
            }
        }
        return nullptr;
    }

    bool HAKCDebugInfoProcessor::isPointerToAnonStructOrUnion(const DIType *diType) {
        /* Remove consts and member tags but keep pointer tags (so can't use unwrapDIType) */
        if (auto *diDerivedType1 = dyn_cast<DIDerivedType>(diType)) {
            if (diType->getTag() != dwarf::DW_TAG_pointer_type) {
                return diDerivedType1->getBaseType() && isPointerToAnonStructOrUnion(diDerivedType1->getBaseType());
            }
        }
        if (diType->getTag() == dwarf::DW_TAG_pointer_type) {
            auto *diDerivedType = dyn_cast<DIDerivedType>(diType);
            return diDerivedType->getBaseType() && isAnonStructOrUnion(diDerivedType->getBaseType());
        }
        return false;
    }

    bool HAKCDebugInfoProcessor::isAnonStructOrUnion(const DIType *diType) {
        const DIType *unwrapped = unwrapDIType(diType);
        if (unwrapped->getTag() == dwarf::DW_TAG_union_type || unwrapped->getTag() == dwarf::DW_TAG_structure_type) {
            if (auto *diCompositeType = dyn_cast<DICompositeType>(unwrapped)) {
                return diCompositeType->getName().empty();
            }
        }
        return false;
    }

    bool HAKCDebugInfoProcessor::isArrayOfAnonStructsOrUnions(const DIType *diType) {
        /* Remove consts and member tags */
        const DIType *unwrapped = unwrapDIType(diType);
        if (unwrapped->getTag() == dwarf::DW_TAG_array_type) {
            auto *diCompositeType = dyn_cast<DICompositeType>(unwrapped);
            return diCompositeType->getBaseType() && isAnonStructOrUnion(diCompositeType->getBaseType());
        }
        return false;
    }
} // hakc
