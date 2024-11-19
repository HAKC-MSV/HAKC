//
// Created by al32163 on 10/23/2024
//

#ifndef HAKC_HAKCALLOCATIONSIZE_H
#define HAKC_HAKCALLOCATIONSIZE_H

#include <memory>
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/YAMLParser.h"
#include "llvm/Support/YAMLTraits.h"

#include "HAKC-defs.h"
#include "HAKCSystemInformation.h"
#include "HAKCSystem/yaml/HAKCYaml.h"

using namespace llvm;

namespace hakc {
    class HAKCAllocationSize {
    public:
        static std::shared_ptr<HAKCAllocationSize> FromYaml(const HAKCYAMLAllocationType& YamlLine, Module &M);
        virtual ConstantInt *GetSize(CallInst *val) = 0;
        Function *GetAllocationFunction();

    protected:
        explicit HAKCAllocationSize(Function *AllocationFunction);
        HAKCAllocationSize() = default;
        Function *AllocationFunction;
    };

    class HAKCSingleArgumentSize : public HAKCAllocationSize {
        friend class HAKCAllocationSize;
    public:
        HAKCSingleArgumentSize(Function *AllocationFunction, const std::vector<std::string> &Arguments);
        ~HAKCSingleArgumentSize() = default;
        ConstantInt *GetSize(CallInst *Val) override;

    protected:
        unsigned ArgNo;
    };

}// namespace hakc

#endif//HAKC_HAKCALLOCATIONSIZE_H
