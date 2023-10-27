//
// Created by de29664 on 4/4/23.
//

#include "HAKCAnalysis/Linux/Arm/HAKCModuleAnalysisLinuxArm.h"

namespace hakc {

    HAKCModuleAnalysisLinuxArm::HAKCModuleAnalysisLinuxArm(Module &M)
            : HAKCModuleAnalysisLinux(M) {

    }

    std::set<StringRef> HAKCModuleAnalysisLinuxArm::GetSeparateNamespacePaths() {
        auto Paths = HAKCModuleAnalysisLinux::GetSeparateNamespacePaths();
        StringRef extras[] = {
                "arch/arm64/kvm/hyp/",
                "arch/arm64/kernel/vdso/",
        };
        return AddToSet(Paths, extras);
    }

    std::set<StringRef> HAKCModuleAnalysisLinuxArm::GetNoTransferFunctions() {
        StringRef extras[] = {
                /* Defined in arch/arm/crypto/aes-neonbs-core.S */
                "aesbs_xts_encrypt",
                "aesbs_xts_decrypt",
                /* Defined in arch/arm64/kernel/sleep.S */
                "_cpu_resume",
                "kretprobe_trampoline",
        };
        auto NoTransferFunctions = HAKCModuleAnalysisLinux::GetNoTransferFunctions();

        return AddToSet(NoTransferFunctions, extras);
    }
} // hakc
