//
// Created by de29664 on 3/22/23.
//

#ifndef HAKC_HAKCTRANSFERCHERI_H
#define HAKC_HAKCTRANSFERCHERI_H

#include "HAKCTransformers/HAKCTransformer.h"
#include "HAKCAnalysis/CheriBSD/HAKCModuleAnalysisCheriBSDCheri.h"

#define HAKC_INIT_ORDER 0x2000000 /* SI_SUB_KLD in CheriBSD */
#define HAKC_ELEM_ORDER 0x0000000 /* SI_ORDER_FIRST in CheriBSD */

namespace hakc {

    /**
     * CheriBSD specific changes for the Arm Morello ISA.
     * The ISA documentation is at https://developer.arm.com/documentation/ddi0606/latest
     */
    class HAKCTransformerCheriBSDCheri : public HAKCTransformer {
    public:
        HAKCTransformerCheriBSDCheri(Module &Module, HAKCModuleAnalysisCheriBSDCheri *Transformation);

        GlobalVariable *AddCompartmentMetadataEntry(hakc_compartment_id_t CompartmentID) override;

        Instruction *CreateCompartmentTransfer(Value *HAKCPointer,
                                               Instruction *I,
                                               Function *Target,
                                               bool IsData) override;

    protected:
        unsigned CapabilityAddressSpace;
        std::map<Function *, LoadInst *> CapabilityLoads;

        /* API implementations */
        Value *CreateSafePointer_Arch(Value *HAKCPointer, Instruction *I) override;

        Type *GetEntryTokenType(unsigned AddrSpace) override;

        Constant *GetEntryToken(hakc_compartment_id_t CompartmentID) override;

        FunctionType *GetHAKCDataAuthenticationFunctionType(unsigned AddrSpace) override;

        bool ValidateHAKCIntegerPointerSize(Value *HAKCPointer) override;

        /* Class Specific members */
        virtual std::string GetSealingCapabilityName(hakc_compartment_id_t CompartmentID);

        virtual CallInst *SaveCompartment(Value *V);

        virtual CallInst *RestoreCompartment(Value *V, Value *OriginalCompartment);

        virtual const StringRef GetSaveCompartmentName();

        virtual const StringRef GetRestoreCompartmentName();

        virtual const StringRef GetSafeCapabilityName();

        virtual const StringRef GetSafePointerName();

        virtual const StringRef GetCompartmentInitName();

        virtual Type *GetCapabilityType();

        std::vector<Value *> CreateArgumentsWithCompartment(Value *HAKCPointer, Function *Target);

        std::vector<Value *> CreateDataAuthArguments(Value *HAKCPointer, Instruction *I) override;

        std::vector<Value *> CreateCodeAuthArguments(Value *HAKCPointer, Instruction *I) override;

        std::vector<Value *> CreateTransferArguments(Value *HAKCPointer, Function *Target, bool IsData,
                                                     ConstantInt *Size) override;

        GlobalValue *GetAccessCapability(hakc_compartment_id_t CompartmentID);

        bool CompilingPureCapKernel() const;

        /**
         * Create a system init entry to initialize the HAKC Sealing Capabilities
         * @param CompartmentID
         */
        void CreateCapabilityReassignment(hakc_compartment_id_t CompartmentID);
    };

} // hakc

#endif //HAKC_HAKCTRANSFERCHERI_H
