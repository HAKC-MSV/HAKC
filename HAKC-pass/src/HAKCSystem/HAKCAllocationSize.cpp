//
// Created by al32163 on 10/23/2024
//

#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKCSystem/HAKCAllocationSize.h"

namespace hakc {

    static ConstantInt *simpleArgumentSize(CallInst *allocation, unsigned ArgNo) {
        IRBuilder<> irBuilder(allocation);
        Value *size = allocation->getArgOperand(ArgNo);
        size = irBuilder.CreateZExtOrBitCast(size, irBuilder.getInt64Ty());
        auto *CI = dyn_cast<ConstantInt>(size);
        return CI;
    }

    ConstantInt *HAKCSingleArgumentSize::GetSize(CallInst *Val) {
        IRBuilder<> irBuilder(Val);
        Value *size = Val->getArgOperand(ArgNo);
        size = irBuilder.CreateZExtOrBitCast(size, irBuilder.getInt64Ty());
        auto *CI = dyn_cast<ConstantInt>(size);
        return CI;
    }

    HAKCSingleArgumentSize::HAKCSingleArgumentSize(Function *AllocationFunction,
                                                   const std::vector<std::string> &Arguments)
            : HAKCAllocationSize(AllocationFunction), ArgNo(0) {
        if (Arguments.empty()) {
            CommonHAKCAnalysis::getWriter() << "Invalid Arguments\n";
            throw std::exception();
        }

        StringRef Arg(Arguments[0]);
        Arg.getAsInteger(10, ArgNo);
    }

    const char *HAKCSingleArgumentSize::YAMLString() {
        return "simpleArgumentSize";
    }

    HAKCAllocationSize::HAKCAllocationSize(Function *AllocationFunction) : AllocationFunction(AllocationFunction) {

    }


    std::unique_ptr<HAKCAllocationSize>
    HAKCAllocationSize::FromYaml(const hakc::HAKCAllocationType &YamlAllocation, Module &M) {
        auto *F = M.getFunction(YamlAllocation.FunctionName);
        if (!F) {
            return nullptr;
        }

        if (YamlAllocation.AllocationType == hakc::SimpleArgumentSize) {
            return std::unique_ptr<HAKCSingleArgumentSize>(new HAKCSingleArgumentSize(F, YamlAllocation.Arguments));
        }

        CommonHAKCAnalysis::getWriter() << "HAKCAllocation Type " << YamlAllocation.AllocationType
                                        << " is not supported\n";
        throw std::exception();

//        if (tokens.size() == 3) {
//            tokens[2].getAsInteger(10, args[0]);
//            // CommonHAKCAnalysis::getWriter() << "in parse found 3 tokens: " << tokens[0] << ", " << tokens[1] << ", " << tokens[2] << "\n";
//        } else if (tokens.size() == 4) {
//            tokens[2].getAsInteger(10, args[0]);
//            tokens[3].getAsInteger(10, args[1]);
//            // CommonHAKCAnalysis::getWriter() << "in parse found 4 tokens: " << tokens[0] << ", " << tokens[1] << ", " << tokens[2] << ", " << tokens[3] << "\n";
//        } else {
//            // CommonHAKCAnalysis::getWriter() << "\t in parse found invalid tokens of size: " << tokens.size() << "\n";
//        }
//
//        return Result;
    }

//    ConstantInt *HAKCAllocationSize::GetSize(CallInst *val) {
//        return nullptr;
//    }

//    Function *HAKCAllocationSize::GetAllocationFunction() {
//        return AllocationFunction;
//    }
//
//    ConstantInt *HAKCAllocationSize::simpleStaticSize(Value *allocation) {
//        return ConstantInt::get(Type::getInt64Ty(allocation->getContext()), args[0], false);
//    }
//
//    ConstantInt *HAKCAllocationSize::staticPlusArgument(Value *allocation) {
//        if (CallInst *call = dyn_cast<CallInst>(allocation)) {
//            ConstantInt *argumentSize = dyn_cast<ConstantInt>(call->getArgOperand(args[1]));
//            return ConstantInt::get(Type::getInt64Ty(allocation->getContext()), argumentSize->getZExtValue() + args[0],
//                                    false);
//        }
//        return nullptr;
//    }
//
//    ConstantInt *HAKCAllocationSize::multiplyTwoArguments(Value *allocation) {
//        if (CallInst *call = dyn_cast<CallInst>(allocation)) {
//            IRBuilder<> irBuilder(call);
//            auto *int64Ty = irBuilder.getInt64Ty();
//            /* Defying all reason, somehow some functions have different argument counts than
//             * expected. See kmalloc_array in the IR for linereq_ioctl. So in that case, take
//             * the lowest argument value.
//             */
//            Value *fullSize = nullptr;
//            if (args[0] >= call->getNumArgOperands() || args[1] >= call->getNumArgOperands()) {
//                if (args[0] <= args[1]) {
//                    fullSize = call->getArgOperand(args[0]);
//                } else {
//                    fullSize = call->getArgOperand(args[1]);
//                }
//            } else {
//                fullSize = irBuilder.CreateMul(
//                        irBuilder.CreateZExt(call->getArgOperand(args[0]), int64Ty),
//                        irBuilder.CreateZExt(call->getArgOperand(args[1]), int64Ty));
//            }
//            fullSize = irBuilder.CreateZExtOrBitCast(fullSize, int64Ty);
//            ConstantInt *CI = dyn_cast<ConstantInt>(fullSize);
//            return CI;
//        }
//
//        return nullptr;
//    }
//
//    ConstantInt *HAKCAllocationSize::argumentGEP(Value *allocation) {
//        if (CallInst *call = dyn_cast<CallInst>(allocation)) {
//            /*HAKCIRBuilder<> irBuilder(call);
//            IntegerType *sizeTy = irBuilder.getInt64Ty();
//            std::vector<Value*> indices;
//            indices.push_back(ConstantInt::get(sizeTy, args[1], false));
//            Value *gep = irBuilder.CreateGEP(sizeTy, call->getArgOperand(args[0]), indices);
//            Value *size = irBuilder.CreateLoad(sizeTy, gep);
//            return size;*/
//
//            // TODO: Fix this
//            return ConstantInt::get(Type::getInt64Ty(allocation->getContext()), 64, false);
//        }
//
//        return nullptr;
//    }
//
//    ConstantInt *HAKCAllocationSize::GetSize(Value *val) {
//        // set args from tokens
//
//        if (tokens[1] == "simpleArgumentSize") {
//            return simpleArgumentSize(val);
//        } else if (tokens[1] == "simpleStaticSize") {
//            return simpleStaticSize(val);
//        } else if (tokens[1] == "multiplyTwoArguments") {
//            return multiplyTwoArguments(val);
//        } else if (tokens[1] == "staticPlusArgument") {
//            return staticPlusArgument(val);
//        } else if (tokens[1] == "argumentGEP") {
//            return argumentGEP(val);
//        } else {
//            // CommonHAKCAnalysis::getWriter() << "tokens[1]: " << tokens[1] << " is not valid type\n";
//            return nullptr;
//        }
//    }


}// namespace hakc
