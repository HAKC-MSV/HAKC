//
// Created by de29664 on 6/21/23.
//

#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"

hakc::HAKCCustomTransfer::HAKCCustomTransfer(Module &M, StringRef TypeName, StringRef TransferFunctionName,
                                             Type *ReturnTy, ArrayRef<Type *> ArgTys, unsigned SignedPtrIdx,
                                             unsigned CompartmentIdIdx, int ColorIdx) :
        HAKCTransferFunction(TransferFunctionName, SignedPtrIdx, CompartmentIdIdx, ColorIdx),
        TargetType(nullptr), CustomTransfer(nullptr) {
    FindTargetTypeAndTransfer(M, TransferFunctionName, TypeName, ReturnTy, ArgTys);
}

hakc::HAKCCustomTransfer::HAKCCustomTransfer(Module &M, StringRef TypeName, StringRef TransferFunctionName,
                                             Type *ReturnTy, ArrayRef<Type *> ArgTys, unsigned int SignedPtrIdx,
                                             unsigned int CompartmentIdIdx) : HAKCTransferFunction
                                                                                      (TransferFunctionName,
                                                                                       SignedPtrIdx, CompartmentIdIdx),
                                                                              TargetType(nullptr),
                                                                              CustomTransfer(nullptr) {
    FindTargetTypeAndTransfer(M, TransferFunctionName, TypeName, ReturnTy, ArgTys);
}

Type *hakc::HAKCCustomTransfer::GetType() const {
    return TargetType;
}

Function *hakc::HAKCCustomTransfer::GetFunction() const {
    return CustomTransfer;
}

void hakc::HAKCCustomTransfer::FindTargetTypeAndTransfer(Module &M, StringRef TransferFunctionName, StringRef TypeName,
                                                         Type *ReturnTy, ArrayRef<Type *> ArgTys) {
    TargetType = StructType::getTypeByName(M.getContext(), TypeName);
    if (TargetType) {
        TargetType = TargetType->getPointerTo();
        if (ReturnTy == nullptr) {
            ReturnTy = TargetType;
        }
        CustomTransfer = M.getFunction(TransferFunctionName);
        if (!CustomTransfer) {
            std::vector<Type *> Params;
            for (auto *Ty: ArgTys) {
                if (Ty) {
                    Params.push_back(Ty);
                } else {
                    Params.push_back(TargetType);
                }
            }
            auto *FuncTy = FunctionType::get(ReturnTy, Params, false);
            auto callee = M.getOrInsertFunction(TransferFunctionName, FuncTy);
            CustomTransfer = dyn_cast<Function>(callee.getCallee());
        }
    }
}
