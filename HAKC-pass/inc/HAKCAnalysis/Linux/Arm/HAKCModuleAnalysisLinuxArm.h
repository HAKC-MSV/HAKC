//
// Created by de29664 on 4/4/23.
//

#ifndef HAKC_HAKCMODULEANALYSISLINUXARM_H
#define HAKC_HAKCMODULEANALYSISLINUXARM_H

#include "HAKCAnalysis/Linux/HAKCModuleAnalysisLinux.h"

namespace hakc {

    class HAKCModuleAnalysisLinuxArm : public HAKCModuleAnalysisLinux {
    public:
        std::set<StringRef> GetNoTransferFunctions() override;

        std::set<StringRef> GetSeparateNamespacePaths() override;

    protected:
        HAKCModuleAnalysisLinuxArm(Module &M);

    };

} // hakc

#endif //HAKC_HAKCMODULEANALYSISLINUXARM_H
