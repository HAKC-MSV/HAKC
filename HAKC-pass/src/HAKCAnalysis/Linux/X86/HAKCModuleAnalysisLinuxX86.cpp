//
// Created by de29664 on 3/31/23.
//

#include "HAKCAnalysis/Linux/X86/HAKCModuleAnalysisLinuxX86.h"
#include "HAKCAnalysis/Linux/X86/HAKCFunctionAnalysisLinuxX86.h"
#include "HAKCTransformers/Linux/X86/HAKCTransformerLinuxX86.h"

namespace hakc {
    HAKCModuleAnalysisLinuxX86::HAKCModuleAnalysisLinuxX86(Module &M) :
            HAKCModuleAnalysisLinux(M) {}

    HAKCFunctionAnalysis *
    HAKCModuleAnalysisLinuxX86::GetFunctionTransformation(Function *F, HAKCCompartmentalizationPolicy &Policy) {
        return new HAKCFunctionAnalysisLinuxX86(F, Policy, this);
    }

    std::shared_ptr<HAKCTransformer>
    HAKCModuleAnalysisLinuxX86::CreateTransformer(HAKCCompartmentalizationPolicy &Policy) {
        return std::make_shared<HAKCTransformerLinuxX86>(Policy, *this);
    }

    std::set<StringRef> HAKCModuleAnalysisLinuxX86::GetSeparateNamespacePaths() {
        return HAKCModuleAnalysisLinux::GetSeparateNamespacePaths();
    }

    std::set<StringRef> HAKCModuleAnalysisLinuxX86::GetNoTransferFunctions() {
        auto Functions = HAKCModuleAnalysisLinux::GetNoTransferFunctions();
        StringRef extras[] = {
                /* arch/x86/include/asm/page_64.h */
                "clear_page_orig",
                "clear_page_rep",
                "clear_page_erms",
                /* arch/x86/include/asm/uaccess_64.h */
                "copy_user_generic_unrolled",
                "copy_user_generic_string",
                "copy_user_enhanced_fast_string",
                "asm_sysvec_xen_hvm_callback",
                "sha256_transform_avx",
                "sha256_transform_ssse3",
                "sha256_transform_avx",
                "sha256_transform_rorx",
                "sha256_ni_transform",
        };
        auto NoTransferFunctions = AddToSet(Functions, extras);

        return NoTransferFunctions;
    }

    std::set<StringRef> HAKCModuleAnalysisLinuxX86::GetHAKCSourcePaths() {
        StringRef extras[] = {
                /* current paths for x86 HAKC */
                "arch/x86/kernel/hakc/hakc_ni.c",
                /* legacy paths for x86 HAKC */
                "arch/x86/kernel/hakc/hakc.c",
                "arch/x86/kernel/hakc/hakc_btree.c",
                "arch/x86/kernel/hakc/hakc_memory.c",
        };

        /* legacy + current paths for common HAKC code */
        auto Paths = HAKCModuleAnalysisLinux::GetHAKCSourcePaths();
        return AddToSet(Paths, extras);
    }

    bool HAKCModuleAnalysisLinuxX86::functionIsTransferCandidate(Function *F, HAKCCompartmentalizationPolicy &Policy) {
        if (F->getName().contains("__SCT")) {
            /* Handle trampolines */
            return false;
        }
        return HAKCModuleAnalysisLinux::functionIsTransferCandidate(F, Policy);
    }

    bool
    HAKCModuleAnalysisLinuxX86::TransferFunctionShouldBeCreated(Function *F, HAKCCompartmentalizationPolicy &Policy) {
        /* There are functions which are declared and then defined by assembly in a macro
        * (see PV_CALLEE_SAVE_REGS_THUNK in arch/x86/include/asm/paravirt.h). So if
        * that is the case, do not create a transfer function */
        if (F->isDeclaration() && FunctionDefinedInAssembly(F)) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << F->getName() << " was found in the Module inline assembly\n";
            }
            return true;
        }
        return HAKCModuleAnalysisLinux::TransferFunctionShouldBeCreated(F, Policy);
    }

    bool HAKCModuleAnalysisLinuxX86::AliasShouldBeCreated(Function *F, HAKCCompartmentalizationPolicy &Policy) {
        /* See note in HAKCModuleAnalysisLinuxX86::TransferFunctionShouldBeCreated */
        if (F->isDeclaration() && FunctionDefinedInAssembly(F)) {
            return false;
        }
        return HAKCModuleAnalysisLinux::AliasShouldBeCreated(F, Policy);
    }
} // hakc
