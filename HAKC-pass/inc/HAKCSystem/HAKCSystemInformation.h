//
// Created by de29664 on 11/7/24.
//

#ifndef HAKC_HAKCSYSTEMINFORMATION_H
#define HAKC_HAKCSYSTEMINFORMATION_H

#include <set>
#include <map>
#include <string>
#include "llvm/Support/YAMLTraits.h"
#include "llvm/IR/Module.h"
#include "HAKCSystem/HAKCAllocationSize.h"
#include "llvm/ADT/ilist.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/IR/ValueMap.h"
#include "HAKCFunctionDefinition/HAKCTransferFunction.h"
#include "HAKCYaml/HAKCYaml.h"

typedef std::shared_ptr<hakc::HAKCAllocationSize> HAKCCustomAllocation;

typedef SmallVector<hakc::hakc_function_def_t> HAKCFunctionList;
typedef SmallVector<hakc::hakc_transfer_def_t> HAKCTransferList;
typedef SmallVector<GlobalVariable*> HAKCGlobalList;
typedef SmallVector<Function*> FunctionList;

namespace hakc {
    class HAKCSystemInformation {
    public:
        explicit HAKCSystemInformation(Module &M);

        friend void operator<<(HAKCSystemInformation &HAKCSystemInfo, HAKCYamlConfig &YamlConfig);

        iterator_range<FunctionList::iterator> GetNoTransferFunctions();
        iterator_range<HAKCTransferList::iterator> CompartmentTransferFunctions();

    protected:
        Module &M;
        std::string Arch;
        std::string Platform;
        std::string Database;
        FunctionList NoTransferFunctionList;
        HAKCTransferList CompartmentTransferFunctionList;
        HAKCFunctionList CompartmentalizationValidationFunctionList;
        FunctionList CompartmentalizationSupportFunctionList;
        StringSet<> SeparateNamespacePaths;
        StringSet<> HAKCSourcePaths;
        FunctionList SafeTransitionFunctionList;
        SmallPtrSet<Type*, 16> IgnoredTypes;
        HAKCGlobalList IgnoredGlobalList;
        ValueMap<Function*, HAKCCustomAllocation> AllocationSizeMap;
    };

} // hakc

#endif //HAKC_HAKCSYSTEMINFORMATION_H
