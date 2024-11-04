//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCTRANSFORMERLINUX_H
#define HAKC_HAKCTRANSFORMERLINUX_H

#include "HAKCTransformers/HAKCTransformer.h"
#include "HAKCAnalysis/Linux/HAKCModuleAnalysisLinux.h"
#include "HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"


namespace hakc {
    class HAKCModuleAnalysisLinux;

    class HAKCTransformerLinux : public HAKCTransformer {
    protected:
        StructType *EntryTokenType;

        HAKCTransformerLinux(HAKCCompartmentalizationPolicy &Policy, HAKCModuleAnalysisLinux &HAKCAnalysis,
                             HAKCTypeIdentifier &TypeIdentifier);

        Value *CreateSafePointer_Arch(ManagedHAKCPointerP HAKCPointer, Instruction *I) override;

        Type *GetEntryTokenType(unsigned AddrSpace) override;

        Constant *GetEntryToken(HAKCCompartment &Compartment) override;

        void CreateTransferFunctionFinalize_Arch(Function *Original, Function *Transfer) override;

        void CreateTransferFunctionArg_PreCall(Function *Target, Function *TransferFunction, Value *Arg) override;

        void CreateTransferFunctionArg_PostCall(Function *Target, Function *TransferFunction, Value *Arg) override;

        FunctionType *GetHAKCDataAuthenticationFunctionType(unsigned AddrSpace) override;

        virtual CallInst *SaveColor(Value *V);

        virtual StringRef HAKCGetColorName();

        virtual StringRef HAKCGetPerCPUColorName();

        virtual StringRef HAKCColorAddressName();

        virtual std::string getUniqueAddressable_Name(Function *F);

        void CreateDataAuthArguments(ManagedHAKCPointerP HAKCPointer, Instruction *I,
                                     SmallVector<Value *> &ArgsList) override;

        void CreateCodeAuthArguments(ManagedHAKCPointerP HAKCPointer, Instruction *I,
                                     SmallVector<Value *> &ArgsList) override;

        void
        CreateTransferArguments(ManagedHAKCPointerP HAKCPointer, GlobalValue *Target, bool IsData, ConstantInt *Size,
                                SmallVector<Value *> &Result) override;

        ConstantInt *GetColorValue(sym_color_t Color);

    private:
        std::map<Value *, CallInst *> TransferArgumentsToRestore;
    };
}

#endif //HAKC_HAKCTRANSFORMERLINUX_H
