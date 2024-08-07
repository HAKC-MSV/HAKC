//
// Created by de29664 on 4/4/23.
//

#ifndef HAKC_HAKCTRANSFORMERLINUXX86_H
#define HAKC_HAKCTRANSFORMERLINUXX86_H

#include "HAKCTransformers/Linux/HAKCTransformerLinux.h"

namespace hakc {

    class HAKCTransformerLinuxX86 : public HAKCTransformerLinux {
    public:
        HAKCTransformerLinuxX86(HAKCCompartmentalizationPolicy &Policy, HAKCModuleAnalysisLinuxX86 &ModAnalysis);
    };

} // hakc

#endif //HAKC_HAKCTRANSFORMERLINUXX86_H
