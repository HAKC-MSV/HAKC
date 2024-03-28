//
// Created by de29664 on 3/31/23.
//

#include "HAKCAnalysis/Linux/X86/HAKCModuleAnalysisLinuxX86.h"
#include "HAKCAnalysis/Linux/X86/HAKCFunctionAnalysisLinuxX86.h"
#include "HAKCTransformers/Linux/X86/HAKCTransformerLinuxX86.h"

namespace hakc {
    HAKCModuleAnalysisLinuxX86::HAKCModuleAnalysisLinuxX86(Module &M) :
            HAKCModuleAnalysisLinux(M) {}

    HAKCFunctionAnalysis *HAKCModuleAnalysisLinuxX86::GetFunctionTransformation(Function *F) {
        return new HAKCFunctionAnalysisLinuxX86(F, this);
    }

    std::shared_ptr<HAKCTransformer> HAKCModuleAnalysisLinuxX86::CreateTransformer() {
        return std::make_shared<HAKCTransformerLinuxX86>(M, this);
    }

    std::set<StringRef> HAKCModuleAnalysisLinuxX86::GetSeparateNamespacePaths() {
        auto Paths = HAKCModuleAnalysisLinux::GetSeparateNamespacePaths();
        StringRef extras[] = {
                /* below are problematic files from x86 that lead to undefined transfer function symbols */
                /* kvm instruction emulation */
                "arch/x86/kvm/emulate.c",
                /* idt */
                "arch/x86/kernel/idt.c",
                /* irq */
                "arch/x86/kernel/irq.c",
                /* ftrace */
                "arch/x86/kernel/ftrace.c",
                /* other problematic sources */
                "arch/x86/kernel/machine_kexec_64.c",
                "arch/x86/kernel/module.c",
                "arch/x86/net/bpf_jit_comp.c",
                "arch/x86/kernel/x86_init.c",
                "arch/x86/kernel/pci-swiotlb.c",
                "arch/x86/kernel/acpi/boot.c"
                /* Hypervisor related sources */
                "arch/x86/xen/apic.c",
                "arch/x86/xen/efi.c",
                "arch/x86/xen/enlighten_hvm.c",
                "arch/x86/xen/enlighten.c",
                "arch/x86/xen/enlighten_pv.c",
                "arch/x86/xen/grant-table.c",
                "arch/x86/xen/irq.c",
                "arch/x86/xen/mmu_hvm.c",
                "arch/x86/xen/mmu.c",
                "arch/x86/xen/mmu_pv.c",
                "arch/x86/xen/multicalls.c",
                "arch/x86/xen/p2m.c",
                "arch/x86/xen/pci-swiotlb-xen.c",
                "arch/x86/xen/platform-pci-unplug.c",
                "arch/x86/xen/pmu.c",
                "arch/x86/xen/setup.c",
                "arch/x86/xen/smp_hvm.c",
                "arch/x86/xen/smp.c",
                "arch/x86/xen/smp_pv.c",
                "arch/x86/xen/suspend_hvm.c",
                "arch/x86/xen/suspend.c",
                "arch/x86/xen/suspend_pv.c",
                "arch/x86/xen/time.c",
                "arch/x86/xen/trace.c",
                "arch/x86/xen/vga.c",
                "arch/x86/xen/xen-asm.c",
                "arch/x86/kernel/paravirt.c",
                "arch/x86/kernel/kvm.c",
                "arch/x86/kernel/cpu/vmware.c",
                /* pass crashes with these */
		/* asm sideeffect something or other */
		"fs/readdir.c",
		"mm/maccess.c",
		"net/core/scm.c",
		"mm/gup.c",
		"arch/x86/kernel/signal_64.c",
		"arch/x86/kvm/x86.c",
		"kernel/rseq.c",
		/* undefined symbol bpf_dispatcher_nop_func */
		"net/core/filter.c",
		/*this breaks v5.15 build if it is commented out */
		/*with it uncommented, KERNEL DOESNT BOOT WHEN COMPARTMENTALIZED */
		/*the no-op common-kernel weak symbol version gets used :-( */
		/*potentially this might work if the weak def (kernel entry common) source is also added */
		"arch/x86/kernel/signal.c",
		"kernel/entry/common.c",
        };
        return AddToSet(Paths, extras);
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

    bool HAKCModuleAnalysisLinuxX86::functionIsTransferCandidate(Function *F) {
        if (F->getName().contains("__SCT")) {
            /* Handle trampolines */
            return false;
        }
        return HAKCModuleAnalysisLinux::functionIsTransferCandidate(F);
    }

    bool HAKCModuleAnalysisLinuxX86::TransferFunctionShouldBeCreated(Function *F) {
        /* There are functions which are declared and then defined by assembly in a macro
        * (see PV_CALLEE_SAVE_REGS_THUNK in arch/x86/include/asm/paravirt.h). So if
        * that is the case, do not create a transfer function */
        if (F->isDeclaration() && FunctionDefinedInAssembly(F)) {
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << F->getName() << " was found in the Module inline assembly\n";
            }
            return true;
        }
        return HAKCModuleAnalysisLinux::TransferFunctionShouldBeCreated(F);
    }

    bool HAKCModuleAnalysisLinuxX86::AliasShouldBeCreated(Function *F) {
        /* See note in HAKCModuleAnalysisLinuxX86::TransferFunctionShouldBeCreated */
        if (F->isDeclaration() && FunctionDefinedInAssembly(F)) {
            return false;
        }
        return HAKCModuleAnalysisLinux::AliasShouldBeCreated(F);
    }
} // hakc
