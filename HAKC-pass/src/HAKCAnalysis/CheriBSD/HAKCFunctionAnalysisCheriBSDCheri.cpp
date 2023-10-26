//
// Created by de29664 on 3/29/23.
//

#include "HAKCAnalysis/CheriBSD/HAKCFunctionAnalysisCheriBSDCheri.h"

namespace hakc {
    HAKCFunctionAnalysisCheriBSDCheri::HAKCFunctionAnalysisCheriBSDCheri(Function *F,
                                                                         HAKCModuleAnalysisCheriBSDCheri *ModTransform)
            :
            HAKCFunctionAnalysis(F, CommonHAKCAnalysis::getHAKCDebugName() == F->getName()),
            ModAnalysis(ModTransform) {

    }

    HAKCModuleAnalysis &HAKCFunctionAnalysisCheriBSDCheri::getModuleAnalysis() {
        return *ModAnalysis;
    }

    Instruction *HAKCFunctionAnalysisCheriBSDCheri::GetFinalAllocaDef(AllocaInst *Alloca) {
        std::set<Intrinsic::ID> IntrinsicDefs = {
                Intrinsic::cheri_cap_bounds_set,
                Intrinsic::cheri_cap_bounds_set_exact,
        };

        std::set<Instruction *> WorkingList = {Alloca};
        while (!WorkingList.empty()) {
            auto *I = *WorkingList.begin();
            WorkingList.erase(I);
            for (auto *U: I->users()) {
                if (auto *Call = dyn_cast<CallInst>(U)) {
                    if (Call->getCalledFunction() && Call->getCalledFunction()->isIntrinsic()) {
                        if (IntrinsicDefs.find(Call->getCalledFunction()->getIntrinsicID()) != IntrinsicDefs.end()) {
                            return Call;
                        }
                    }
                } else if (auto *BitCast = dyn_cast<BitCastInst>(U)) {
                    WorkingList.insert(BitCast);
                }
            }
        }

        return HAKCFunctionAnalysis::GetFinalAllocaDef(Alloca);
    }

    std::set<Intrinsic::ID> HAKCFunctionAnalysisCheriBSDCheri::GetInstrinsicsToSkip() {
        auto Intrinsics = HAKCFunctionAnalysis::GetInstrinsicsToSkip();

        Intrinsic::ID AdditionalIDs[] = {
                Intrinsic::cheri_bounded_stack_cap,
                Intrinsic::cheri_bounded_stack_cap_dynamic,
                Intrinsic::cheri_cap_address_get,
                Intrinsic::cheri_cap_address_set,
                Intrinsic::cheri_cap_base_get,
                Intrinsic::cheri_cap_bounds_set,
                Intrinsic::cheri_cap_bounds_set_exact,
                Intrinsic::cheri_cap_build,
                Intrinsic::cheri_cap_conditional_seal,
                Intrinsic::cheri_cap_copy_from_high,
                Intrinsic::cheri_cap_copy_to_high,
                Intrinsic::cheri_cap_diff,
                Intrinsic::cheri_cap_equal_exact,
                Intrinsic::cheri_cap_flags_get,
                Intrinsic::cheri_cap_flags_set,
                Intrinsic::cheri_cap_from_pointer,
                Intrinsic::cheri_cap_from_pointer_nonnull_zero,
                Intrinsic::cheri_cap_length_get,
                Intrinsic::cheri_cap_load_tags,
                Intrinsic::cheri_cap_offset_get,
                Intrinsic::cheri_cap_offset_set,
                Intrinsic::cheri_cap_perms_and,
                Intrinsic::cheri_cap_perms_check,
                Intrinsic::cheri_cap_perms_get,
                Intrinsic::cheri_cap_sealed_get,
                Intrinsic::cheri_cap_seal,
                Intrinsic::cheri_cap_seal_entry,
                Intrinsic::cheri_cap_subset_test,
                Intrinsic::cheri_cap_tag_clear,
                Intrinsic::cheri_cap_tag_get,
                Intrinsic::cheri_cap_to_pointer,
                Intrinsic::cheri_cap_type_check,
                Intrinsic::cheri_cap_type_copy,
                Intrinsic::cheri_cap_type_get,
                Intrinsic::cheri_cap_unseal,
                Intrinsic::cheri_ddc_get,
                Intrinsic::cheri_pcc_get,
                Intrinsic::cheri_representable_alignment_mask,
                Intrinsic::cheri_round_representable_length,
                Intrinsic::cheri_stack_cap_get,
        };

        for (auto ID: AdditionalIDs) {
            Intrinsics.insert(ID);
        }

        return Intrinsics;
    }

    void HAKCFunctionAnalysisCheriBSDCheri::handleComparison(CmpInst *compare) {
        /* Since pointers are not signed for Cheri, comparisons can happen normally */
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Checking comparison ";
            compare->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }

        MaybeAddCompareToDirectUsers(compare);
    }

    bool HAKCFunctionAnalysisCheriBSDCheri::pointerShouldBeChecked(Value *ptr) {
        auto *Def = HAKCFunctionAnalysis::getDef(ptr, false, debug_output);
        if (TypeMatchesIgnoredTypes(Def->getType())) {
            return false;
        }
        if (auto *Call = dyn_cast<CallInst>(Def)) {
            if (Call->getCalledFunction() && Call->getCalledFunction()->isIntrinsic()) {
                std::set<Intrinsic::ID> IDsToIgnore = {
                        Intrinsic::cheri_cap_bounds_set,
                        Intrinsic::cheri_cap_bounds_set_exact,
                };
                if (IDsToIgnore.find(Call->getCalledFunction()->getIntrinsicID()) != IDsToIgnore.end()) {
                    return false;
                }
            }
        }

        if (auto *Phi = dyn_cast<PHINode>(Def)) {
            for (unsigned i = 0; i < Phi->getNumIncomingValues(); i++) {
                auto *PhiNodeV = getDef(Phi->getIncomingValue(i), false, debug_output);
                if (TypeMatchesIgnoredTypes(PhiNodeV->getType())) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "PHI Node Value ";
                        PhiNodeV->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << " is an ignored type\n";
                    }
                    return false;
                } else if (auto *LoadI = dyn_cast<LoadInst>(PhiNodeV)) {
                    auto *LoadPtrV = getDef(LoadI->getPointerOperand(), false, debug_output);
                    Type *LoadPtrVTy = LoadPtrV->getType();
                    if (isa<PointerType>(LoadPtrVTy)) {
                        LoadPtrVTy = LoadPtrVTy->getPointerElementType();
                    }
                    if (TypeMatchesIgnoredTypes(LoadPtrVTy)) {
                        if (debug_output) {
                            CommonHAKCAnalysis::getWriter() << "Load Pointer PHI Value ";
                            LoadPtrV->print(CommonHAKCAnalysis::getWriter());
                            CommonHAKCAnalysis::getWriter() << " is an ignored type\n";
                        }
                        return false;
                    }
                    if (debug_output) {
                        LoadPtrV->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << " is not an ignored Type (";
                        LoadPtrV->getType()->getPointerElementType()->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << ")\n";
                        if (auto *GEPI = dyn_cast<GetElementPtrInst>(LoadI->getPointerOperand())) {
                            CommonHAKCAnalysis::getWriter() << "GEP ResultType = ";
                            GEPI->getResultElementType()->print(CommonHAKCAnalysis::getWriter());
                            CommonHAKCAnalysis::getWriter() << "\n";
                        }
                    }
                }
            }
        }

        return HAKCFunctionAnalysis::pointerShouldBeChecked(ptr);
    }

    bool HAKCFunctionAnalysisCheriBSDCheri::TypeMatchesIgnoredTypes(Type *Ty) {
        /* These structs use uintptr_t for a lock value, but they are
        * treated as plain integers. Ignore these types. */
        StringRef TypeNames[] = {
                "struct.rwlock",
                "struct.sx",
                "struct.lock",
        };
        if (auto *StructTy = dyn_cast<StructType>(Ty)) {
            if (StructTy->hasName()) {
                for (auto TypeName: TypeNames) {
                    if (TypeName == StructTy->getName()) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

} // hakc