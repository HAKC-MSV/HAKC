//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCTRANSFORMERLINUX_H
#define HAKC_HAKCTRANSFORMERLINUX_H

#include "HAKCTransformers/HAKCTransformer.h"
#include "HAKCAnalysis/Linux/HAKCModuleAnalysisLinux.h"


namespace hakc {
    class HAKCModuleAnalysisLinux;

    class HAKCTransformerLinux : public HAKCTransformer {
    public:
        bool FunctionIsExported(Function *F) override;

    protected:
        HAKCTransformerLinux(Module &Module, HAKCModuleAnalysisLinux *ModuleAnalysis);

        Value *CreateSafePointer_Arch(Value *HAKCPointer, Instruction *I) override;

        Type *GetEntryTokenType(unsigned AddrSpace) override;

        Constant *GetEntryToken(int64_t CompartmentID) override;

        void CreateTransferFunctionFinalize_Arch(Function *Original, Function *Transfer) override;

        void CreateTransferFunctionArg_PreCall(Function *Target, Function *TransferFunction, Value *Arg) override;

        void CreateTransferFunctionArg_PostCall(Function *Target, Function *TransferFunction, Value *Arg) override;

        FunctionType *GetHAKCDataAuthenticationFunctionType(unsigned AddrSpace) override;

    protected:
        StructType *EntryTokenType;

        virtual CallInst *SaveColor(Value *V);

        virtual const StringRef HAKCGetColorName();

        virtual const StringRef HAKCGetPerCPUColorName();

        virtual const StringRef HAKCColorAddressName();

        virtual std::string getUniqueAddressable_Name(Function *F);

        virtual std::string getKstrtab_entry_name(Function *F);

        virtual std::string getKstrtabns_entry_name(Function *F);

        std::vector<Value *> CreateDataAuthArguments(Value *HAKCPointer, Instruction *I) override;

        std::vector<Value *> CreateCodeAuthArguments(Value *HAKCPointer, Instruction *I) override;

        std::vector<Value *> CreateTransferArguments(Value *HAKCPointer, Function *Target, bool IsData,
                                                     ConstantInt *Size) override;

        ConstantInt *GetColorValue(sym_color_t Color);

    private:
        std::map<Value *, CallInst *> TransferArgumentsToRestore;
    };
}


#endif //HAKC_HAKCTRANSFORMERLINUX_H
