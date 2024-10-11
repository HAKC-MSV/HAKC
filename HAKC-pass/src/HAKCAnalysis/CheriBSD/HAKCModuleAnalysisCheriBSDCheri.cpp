//
// Created by de29664 on 3/29/23.
//

#include "llvm/IR/InstIterator.h"

#include "HAKCTransformers/CheriBSD/HAKCTransformerCheriBSDCheri.h"
#include "HAKCAnalysis/CheriBSD/HAKCModuleAnalysisCheriBSDCheri.h"
#include "HAKCAnalysis/CheriBSD/HAKCFunctionAnalysisCheriBSDCheri.h"

namespace hakc {
    HAKCModuleAnalysisCheriBSDCheri::HAKCModuleAnalysisCheriBSDCheri(Module &Module)
            : HAKCModuleAnalysis(Module) {
    }

    void HAKCModuleAnalysisCheriBSDCheri::InitHAKCFunctions() {
        HAKC_FUNCTION("get_hakc_compartment_id");
        HAKC_TRANSFER_NO_COLOR("hakc_xfer_to_compartment", 1);
    }

    std::shared_ptr<HAKCTransformer> HAKCModuleAnalysisCheriBSDCheri::CreateTransformer() {
        return std::make_shared<HAKCTransformerCheriBSDCheri>(M, this);
    }

    std::set<StringRef> HAKCModuleAnalysisCheriBSDCheri::GetNoTransferFunctions() {
        auto funcs = this->GetSafeTransitionFunctions_Arch();
        std::set<StringRef> s(funcs.begin(), funcs.end());
        StringRef AdditionalFuncs[] = {
                "mi_startup",
                "init_main",
                "g_load_class",
                "cheri_init_capabilities",
                "create_pagetables",
        };
        return AddToSet(s, AdditionalFuncs);
    }

    bool HAKCModuleAnalysisCheriBSDCheri::FunctionNeedsAnalysis(Function *F) {
        auto NoTransferFunctions = GetNoTransferFunctions();
        bool NeedsAnalysis = NoTransferFunctions.find(F->getName()) == NoTransferFunctions.end() &&
                             HAKCModuleAnalysis::FunctionNeedsAnalysis(F);
        return NeedsAnalysis;
    }

    HAKCFunctionAnalysis *HAKCModuleAnalysisCheriBSDCheri::GetFunctionTransformation(Function *F) {
        auto *FunctionAnalysis = new HAKCFunctionAnalysisCheriBSDCheri(F, this);
        return FunctionAnalysis;
    }

    StructType *HAKCModuleAnalysisCheriBSDCheri::GetKernelParamType() {
        return nullptr;
    }

    void HAKCModuleAnalysisCheriBSDCheri::generateModuleParamGetCtxFunction(GlobalVariable *GV) {
        return;
    }

    void HAKCModuleAnalysisCheriBSDCheri::transferModuleParams() {
        return;
    }

    std::vector<StringRef> HAKCModuleAnalysisCheriBSDCheri::GetSafeTransitionFunctions_Arch() {
        return {
                /* These are all assembly functions */
                "generic_bs_r_1",
                "generic_bs_r_2",
                "generic_bs_r_4",
                "generic_bs_r_8",
                "generic_bs_rm_1",
                "generic_bs_rm_2",
                "generic_bs_rm_4",
                "generic_bs_rm_8",
                "generic_bs_rr_1",
                "generic_bs_rr_2",
                "generic_bs_rr_4",
                "generic_bs_rr_8",
                "generic_bs_w_1",
                "generic_bs_w_2",
                "generic_bs_w_4",
                "generic_bs_w_8",
                "generic_bs_wm_1",
                "generic_bs_wm_2",
                "generic_bs_wm_4",
                "generic_bs_wm_8",
                "generic_bs_wr_1",
                "generic_bs_wr_2",
                "generic_bs_wr_4",
                "generic_bs_wr_8",
                "generic_bs_peek_1",
                "generic_bs_peek_2",
                "generic_bs_peek_4",
                "generic_bs_peek_8",
                "generic_bs_poke_1",
                "generic_bs_poke_2",
                "generic_bs_poke_4",
                "generic_bs_poke_8",
                "arm64_dic_idc_icache_sync_range",
                "arm64_aliasing_icache_sync_range",
                "pagezero_cache",
                "pagezero_simple",
                "casueword32_lse",
                "casueword32_llsc",
                "casueword_lse",
                "casueword_llsc",
                "arm_smccc_hvc",
                "arm_smccc_smc",
                "copyin",
                "copyout",
                "mpentry",
                "generic_bs_fault",
                "fork_trampoline",
        };
    }

    std::map<StringRef, hakc_allocation_size_map_t> HAKCModuleAnalysisCheriBSDCheri::GetKernelAllocationSizeMap() {
        return {
                {"malloc", simpleArgumentSize<0>},
        };
    }

    std::set<StringRef> HAKCModuleAnalysisCheriBSDCheri::GetIgnoredTypes() {
        return {};
    }

    std::set<StringRef> HAKCModuleAnalysisCheriBSDCheri::GetHAKCSourcePaths() {
        return {
                "sys/hakc/hakc.c"
        };
    }

    std::set<StringRef> HAKCModuleAnalysisCheriBSDCheri::GetSeparateNamespacePaths() {
        return {
            "sys/arm64/arm64/elf_machdep.c",
            "sys/arm64/arm64/machdep.c",
            "sys/arm64/arm64/machdep_boot.c",
            "sys/contrib/libfdt/fdt.c",
//            "sys/kern/subr_prf.c",
        };
    }

    StringRef HAKCModuleAnalysisCheriBSDCheri::HACKCodeAuthenticationName() {
        return "hakc_check_code_access";
    }

    StringRef HAKCModuleAnalysisCheriBSDCheri::HAKCDataAuthenticationName() {
        return "hakc_check_data_access";
    }

    StringRef HAKCModuleAnalysisCheriBSDCheri::HAKCCompartmentTransferName() {
        return "hakc_xfer_to_compartment";
    }

    StringRef HAKCModuleAnalysisCheriBSDCheri::HAKCCompartmentTransferNoCapName() {
        return "hakc_xfer_to_compartment_nocap";
    }

    Function *HAKCModuleAnalysisCheriBSDCheri::GetFunctionByName(StringRef Name, FunctionType *FuncTy) {
        /* For Hybrid Morello, there is a separate function for transferring regular pointers versus __capability
         * pointers */
        if (Name == HAKCCompartmentTransferName() && (HybridModeEnabled() && FuncTy->getReturnType()
                                                                                     ->getPointerAddressSpace() == 0)) {
            Name = HAKCCompartmentTransferNoCapName();
        }

        return HAKCModuleAnalysis::GetFunctionByName(Name, FuncTy);
    }

    bool HAKCModuleAnalysisCheriBSDCheri::HybridModeEnabled() {
        const auto *env_var = std::getenv(HAKC_COMPARTMENT_PATH.c_str());
        if (env_var) {
            return std::strcmp(env_var, "1") == 0;
        }

        return false;
    }
} // hakc
