//
// Created by de29664 on 6/21/23.
//

#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"

#include <utility>

hakc::HAKCCustomTransfer::HAKCCustomTransfer(Function *CustomFunction, hakc::HAKCTypeP TargetType,
                                             unsigned int SignedPtrIdx, unsigned int CompartmentIdIdx,
                                             int DivisionIdx) : HAKCTransferFunction(CustomFunction, SignedPtrIdx,
                                                                                     CompartmentIdIdx, DivisionIdx),
                                                                TargetType(std::move(TargetType)) {

}

hakc::HAKCCustomTransfer::HAKCCustomTransfer(Function *CustomFunction, hakc::HAKCTypeP TargetType,
                                             unsigned int SignedPtrIdx, unsigned int CompartmentIdIdx,
                                             unsigned int DivisionIdx, unsigned int SizeIdx) : HAKCTransferFunction(
        CustomFunction, SignedPtrIdx, CompartmentIdIdx, DivisionIdx, SizeIdx), TargetType(std::move(TargetType)) {

}

hakc::HAKCTypeP hakc::HAKCCustomTransfer::GetTargetType() const {
    return TargetType;
}
