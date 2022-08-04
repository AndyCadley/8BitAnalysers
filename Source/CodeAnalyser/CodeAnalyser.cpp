#include "CodeAnalyser.h"

#include <cstdint>
#include <algorithm>
#include <cassert>
#include <util/z80dasm.h>
#include <util/m6502dasm.h>

bool CheckPointerIndirectionInstructionZ80(ICPUInterface* pCPUInterface, uint16_t pc, uint16_t* out_addr)
{
	const uint8_t instrByte = pCPUInterface->ReadByte(pc);

	switch (instrByte)
	{
		// LD (nnnn),x
		case 0x22:
		case 0x32:
			// LD x,(nnnn)
		case 0x2A:
		case 0x3A:
			*out_addr = (pCPUInterface->ReadByte(pc + 2) << 8) | pCPUInterface->ReadByte(pc + 1);
			return true;
			// extended instructions
		case 0xED:
		{
			const uint8_t exInstrByte = pCPUInterface->ReadByte(pc + 1);
			switch (exInstrByte)
			{
			case 0x43://ld (**),bc
			case 0x4B://ld bc,(**)
			case 0x53://ld (**),de
			case 0x5B://ld de,(**)
			case 0x63://ld (**),hl
			case 0x6B://ld hl,(**)
			case 0x73://ld (**),sp
			case 0x7B://ld sp,(**)
				*out_addr = (pCPUInterface->ReadByte(pc + 3) << 8) | pCPUInterface->ReadByte(pc + 2);
				return true;
			}

		}

		// IX/IY instructions
		case 0xDD:
		case 0xFD:
		{
			const uint8_t exInstrByte = pCPUInterface->ReadByte(pc + 1);
			switch (exInstrByte)
			{
			case 0x22://ld (**),ix/iy
			case 0x2A://ld ix/iy,(**)
				*out_addr = (pCPUInterface->ReadByte(pc + 3) << 8) | pCPUInterface->ReadByte(pc + 2);
				return true;
			}
		}
	}

	return false;
}

bool CheckPointerIndirectionInstruction(ICPUInterface* pCPUInterface, uint16_t pc, uint16_t* out_addr)
{
	if (pCPUInterface->CPUType == ECPUType::Z80)
		return CheckPointerIndirectionInstructionZ80(pCPUInterface, pc, out_addr);
	else
		return false;
}

bool CheckPointerRefInstructionZ80(ICPUInterface* pCPUInterface, uint16_t pc, uint16_t* out_addr)
{
	if (CheckPointerIndirectionInstructionZ80(pCPUInterface, pc, out_addr))
		return true;
	
	const uint8_t instrByte = pCPUInterface->ReadByte(pc);

	switch (instrByte)
	{
		// LD x,nnnn
		case 0x01:
		case 0x11:
		case 0x21:
			*out_addr = (pCPUInterface->ReadByte(pc + 2) << 8) | pCPUInterface->ReadByte(pc + 1);
			return true;

		// IX/IY instructions
		case 0xDD:
		case 0xFD:
		{
			const uint8_t exInstrByte = pCPUInterface->ReadByte(pc + 1);
			switch (exInstrByte)
			{
			case 0x21://ld ix/iy,**
				*out_addr = (pCPUInterface->ReadByte(pc + 3) << 8) | pCPUInterface->ReadByte(pc + 2);
				return true;
			}
		}
	}
	return false;
}

bool CheckPointerRefInstruction(ICPUInterface* pCPUInterface, uint16_t pc, uint16_t* out_addr)
{
	if (pCPUInterface->CPUType == ECPUType::Z80)
		return CheckPointerRefInstructionZ80(pCPUInterface, pc, out_addr);
	else
		return false;
}

bool CheckJumpInstructionZ80(ICPUInterface* pCPUInterface, uint16_t pc, uint16_t* out_addr)
{
	const uint8_t instrByte = pCPUInterface->ReadByte(pc);

	switch (instrByte)
	{
		/* CALL nnnn */
	case 0xCD:
		/* CALL cc,nnnn */
	case 0xDC: case 0xFC: case 0xD4: case 0xC4:
	case 0xF4: case 0xEC: case 0xE4: case 0xCC:
		/* JP nnnn */
	case 0xC3:
		/* JP cc,nnnn */
	case 0xDA: case 0xFA: case 0xD2: case 0xC2:
	case 0xF2: case 0xEA: case 0xE2: case 0xCA:
		*out_addr = (pCPUInterface->ReadByte(pc + 2) << 8) | pCPUInterface->ReadByte( pc + 1);
		return true;

		/* DJNZ d */
	case 0x10:
		/* JR d */
	case 0x18:
		/* JR cc,d */
	case 0x38: case 0x30: case 0x20: case 0x28:
	{
		const int8_t relJump = (int8_t)pCPUInterface->ReadByte(pc + 1);
		*out_addr = pc + 2 + relJump;	// +2 because it's relative to the next instruction
	}
		return true;
		/* RST */
	case 0xC7:  *out_addr = 0x00; return true;
	case 0xCF:  *out_addr = 0x08; return true;
	case 0xD7:  *out_addr = 0x10; return true;
	case 0xDF:  *out_addr = 0x18; return true;
	case 0xE7:  *out_addr = 0x20; return true;
	case 0xEF:  *out_addr = 0x28; return true;
	case 0xF7:  *out_addr = 0x30; return true;
	case 0xFF:  *out_addr = 0x38; return true;
	}

	return false;
}

bool CheckJumpInstruction(ICPUInterface* pCPUInterface, uint16_t pc, uint16_t* out_addr)
{
	if (pCPUInterface->CPUType == ECPUType::Z80)
		return CheckJumpInstructionZ80(pCPUInterface, pc, out_addr);
	else
		return false;
}


bool CheckCallInstructionZ80(ICPUInterface* pCPUInterface, uint16_t pc)
{
	const uint8_t instrByte = pCPUInterface->ReadByte(pc);

	switch (instrByte)
	{
		/* CALL nnnn */
	case 0xCD:
		/* CALL cc,nnnn */
	case 0xDC: case 0xFC: case 0xD4: case 0xC4:
	case 0xF4: case 0xEC: case 0xE4: case 0xCC:
		/* RST */
	case 0xC7:
	case 0xCF:
	case 0xD7:
	case 0xDF:
	case 0xE7:
	case 0xEF:
	case 0xF7:
	case 0xFF:
		return true;
	}
	return false;
}

bool CheckCallInstruction(ICPUInterface* pCPUInterface, uint16_t pc)
{
	if (pCPUInterface->CPUType == ECPUType::Z80)
		return CheckCallInstructionZ80(pCPUInterface, pc);
	else
		return false;
}

// check if function should stop static analysis
// this would be a function that unconditionally affects the PC
bool CheckStopInstructionZ80(ICPUInterface* pCPUInterface, uint16_t pc)
{
	const uint8_t instrByte = pCPUInterface->ReadByte(pc);

	switch(instrByte)
	{
		/* CALL nnnn */
	case 0xCD:
		/* CALL cc,nnnn */
	case 0xDC: case 0xFC: case 0xD4: case 0xC4:
	case 0xF4: case 0xEC: case 0xE4: case 0xCC:
		/* RST */
	case 0xC7:
	case 0xCF:
	case 0xD7:
	case 0xDF:
	case 0xE7:
	case 0xEF:
	case 0xF7:
	case 0xFF:
		// ret
	case 0xC8:
	case 0xC9:
	case 0xD8:
	case 0xE8:
	case 0xF8:
	
		
	case 0xc3:// jp
	case 0xe9:// jp(hl)
	case 0x18://jr
		return true;
	case 0xED:	// extended instructions
	{
		const uint8_t extInstrByte = pCPUInterface->ReadByte(pc+1);
		switch(extInstrByte)
		{
		case 0x4D:	// more RET functions
		case 0x5D:
		case 0x6D:
		case 0x7D:
		case 0x45:	
		case 0x55:
		case 0x65:
		case 0x75:
			return true;
		}
	}
	// index register instructions
	case 0xDD:	// IX
	case 0xFD:	// IY
	{
		const uint8_t extInstrByte = pCPUInterface->ReadByte(pc + 1);
		switch (extInstrByte)
		{
		case 0xE9:	// JP(IX)
			return true;
		}
	}
	default:
		return false;
	}
}

bool CheckStopInstruction(ICPUInterface* pCPUInterface, uint16_t pc)
{
	if (pCPUInterface->CPUType == ECPUType::Z80)
		return CheckStopInstructionZ80(pCPUInterface, pc);
	else
		return false;
}

bool GenerateLabelForAddress(FCodeAnalysisState &state, uint16_t pc, LabelType labelType)
{
	FLabelInfo* pLabel = state.Labels[pc];
	if (pLabel == nullptr)
	{
		pLabel = new FLabelInfo;
		pLabel->LabelType = labelType;
		pLabel->Address = pc;
		pLabel->ByteSize = 0;
		
		char label[32];
		switch (labelType)
		{
		case LabelType::Function:
			sprintf(label, "function_%04X", pc);
			break;
		case LabelType::Code:
			sprintf(label, "label_%04X", pc);
			break;
		case LabelType::Data:
			sprintf(label, "data_%04X", pc);
			pLabel->Global = true;
			break;
		}

		pLabel->Name = label;
		state.Labels[pc] = pLabel;
		return true;
	}

	return false;
}


struct FAnalysisDasmState
{
	ICPUInterface*	CPUInterface;
	uint16_t		CurrentAddress;
	std::string		Text;
};


/* disassembler callback to fetch the next instruction byte */
static uint8_t AnalysisDasmInputCB(void* pUserData)
{
	FAnalysisDasmState* pDasmState = (FAnalysisDasmState*)pUserData;

	const uint8_t val = pDasmState->CPUInterface->ReadByte( pDasmState->CurrentAddress++);

	// push into binary buffer
	//if (pDasm->bin_pos < DASM_MAX_BINLEN)
	//	pDasm->bin_buf[pDasm->bin_pos++] = val;

	return val;
}

/* disassembler callback to output a character */
static void AnalysisOutputCB(char c, void* pUserData)
{
	FAnalysisDasmState* pDasmState = (FAnalysisDasmState*)pUserData;

	// add character to string
	pDasmState->Text += c;
}

void UpdateCodeInfoForAddress(FCodeAnalysisState &state, uint16_t pc)
{
	FAnalysisDasmState dasmState;
	dasmState.CPUInterface = state.CPUInterface;
	dasmState.CurrentAddress = pc;
	if(state.CPUInterface->CPUType == ECPUType::Z80)
		z80dasm_op(pc, AnalysisDasmInputCB, AnalysisOutputCB, &dasmState);
	else if(state.CPUInterface->CPUType == ECPUType::M6502)
		m6502dasm_op(pc, AnalysisDasmInputCB, AnalysisOutputCB, &dasmState);

	FCodeInfo *pCodeInfo = state.CodeInfo[pc];
	assert(pCodeInfo != nullptr);
	pCodeInfo->Text = dasmState.Text;
}


uint16_t WriteCodeInfoForAddress(FCodeAnalysisState &state, uint16_t pc)
{
	FAnalysisDasmState dasmState;
	dasmState.CPUInterface = state.CPUInterface;
	dasmState.CurrentAddress = pc;
	uint16_t newPC = pc;

	if (state.CPUInterface->CPUType == ECPUType::Z80)
		newPC = z80dasm_op(pc, AnalysisDasmInputCB, AnalysisOutputCB, &dasmState);
	else if(state.CPUInterface->CPUType == ECPUType::M6502)
		newPC = m6502dasm_op(pc, AnalysisDasmInputCB, AnalysisOutputCB, &dasmState);

	FCodeInfo *pCodeInfo = state.CodeInfo[pc];
	if (pCodeInfo == nullptr)
	{
		pCodeInfo = new FCodeInfo;
		state.CodeInfo[pc] = pCodeInfo;
	}

	pCodeInfo->Text = dasmState.Text;
	pCodeInfo->Address = pc;
	pCodeInfo->ByteSize = newPC - pc;
	for(uint16_t codeAddr=pc;codeAddr<newPC;codeAddr++)
		state.CodeInfo[codeAddr] = pCodeInfo;	// just set to first instruction?

	// does this function branch?
	uint16_t jumpAddr;
	if (CheckJumpInstruction(state.CPUInterface, pc, &jumpAddr))
	{
		const bool isCall = CheckCallInstruction(state.CPUInterface, pc);
		if (GenerateLabelForAddress(state, jumpAddr, isCall ? LabelType::Function : LabelType::Code))
			state.Labels[jumpAddr]->References[pc]++;

		pCodeInfo->JumpAddress = jumpAddr;
	}

	uint16_t ptr;
	if (CheckPointerRefInstruction(state.CPUInterface, pc, &ptr))
		pCodeInfo->PointerAddress = ptr;

	if (CheckPointerIndirectionInstruction(state.CPUInterface, pc, &ptr))
	{
		if (GenerateLabelForAddress(state, ptr, LabelType::Data))
			state.Labels[ptr]->References[pc]++;
	}

	return newPC;
}

// return if we should continue
bool AnalyseAtPC(FCodeAnalysisState &state, uint16_t& pc)
{
	// update branch reference counters
	uint16_t jumpAddr;
	if (CheckJumpInstruction(state.CPUInterface, pc, &jumpAddr))
	{
		FLabelInfo* pLabel = state.Labels[jumpAddr];
		if (pLabel != nullptr)
			pLabel->References[pc]++;	// add/increment reference
	}

	uint16_t ptr;
	if (CheckPointerRefInstruction(state.CPUInterface, pc, &ptr))
	{
		FLabelInfo* pLabel = state.Labels[ptr];
		if (pLabel != nullptr)
			pLabel->References[pc]++;	// add/increment reference
	}

	if (state.CodeInfo[pc] != nullptr)	// already been analysed
		return false;

	uint16_t newPC = WriteCodeInfoForAddress(state, pc);
	if (CheckStopInstruction(state.CPUInterface, pc) || newPC < pc)
		return false;
	
	pc = newPC;
	state.bCodeAnalysisDataDirty = true;
	return true;
}

// TODO: make this use above function
void AnalyseFromPC(FCodeAnalysisState &state, uint16_t pc)
{
	//FSpeccy *pSpeccy = state.pSpeccy;

	// update branch reference counters
	/*uint16_t jumpAddr;
	if (CheckJumpInstruction(pSpeccy, pc, &jumpAddr))
	{
		FLabelInfo* pLabel = state.Labels[jumpAddr];
		if (pLabel != nullptr)
			pLabel->References[pc]++;	// add/increment reference
	}

	uint16_t ptr;
	if (CheckPointerRefInstruction(pSpeccy, pc, &ptr))
	{
		FLabelInfo* pLabel = state.Labels[ptr];
		if (pLabel != nullptr)
			pLabel->References[pc]++;	// add/increment reference
	}*/

	if (state.CodeInfo[pc] != nullptr)	// already been analysed
		return;

	state.bCodeAnalysisDataDirty = true;

	
	bool bStop = false;

	while (bStop == false)
	{
		uint16_t jumpAddr = 0;
		const uint16_t newPC = WriteCodeInfoForAddress(state, pc);

		if (CheckJumpInstruction(state.CPUInterface, pc, &jumpAddr))
		{
			AnalyseFromPC(state, jumpAddr);
			bStop = false;	// should just be call & rst really
		}

		// do we need to stop tracing ??
		if (CheckStopInstruction(state.CPUInterface, pc) || newPC < pc)
			bStop = true;
		else
			pc = newPC;
	}
}

void RegisterCodeExecuted(FCodeAnalysisState &state, uint16_t pc)
{
	AnalyseAtPC(state, pc);
}

void RunStaticCodeAnalysis(FCodeAnalysisState &state, uint16_t pc)
{
	//const uint8_t instrByte = ReadSpeccyByte(pUI->pSpeccy, pc);
	
	AnalyseFromPC(state, pc);
	
	//if (bRead)
	//	pUI->MemStats.ReadCount[addr]++;
	//if (bWrite)
	//	pUI->MemStats.WriteCount[addr]++;
	//
}

void RegisterDataAccess(FCodeAnalysisState &state, uint16_t pc,uint16_t dataAddr, bool bWrite)
{
	if (bWrite)
	{
		state.DataInfo[dataAddr]->LastFrameWritten = state.CurrentFrameNo;
		state.DataInfo[dataAddr]->Writes[pc]++;
	}
	else
	{
		state.DataInfo[dataAddr]->LastFrameRead = state.CurrentFrameNo;
		state.DataInfo[dataAddr]->Reads[pc]++;
	}
}

void ReAnalyseCode(FCodeAnalysisState &state)
{
	int nextItemAddress = 0;
	for (int i = 0; i < (1 << 16); i++)
	{
		if (i == nextItemAddress)
		{
			if (state.CodeInfo[i] != nullptr)
			{
				nextItemAddress = WriteCodeInfoForAddress(state, (uint16_t)i);
			}

			if (state.CodeInfo[i] == nullptr && state.DataInfo[i] == nullptr)
			{
				// set up data entry for address
				FDataInfo *pDataInfo = new FDataInfo;
				pDataInfo->Address = (uint16_t)i;
				pDataInfo->ByteSize = 1;
				pDataInfo->DataType = DataType::Byte;
				state.DataInfo[i] = pDataInfo;
			}
		}

		if ((state.CodeInfo[i] != nullptr) && (state.Labels[i] != nullptr) && (state.Labels[i]->LabelType == LabelType::Data))
		{
			state.CodeInfo[i]->bSelfModifyingCode = true;
		}
	}
}

void ResetMemoryLogs(FCodeAnalysisState &state)
{
	for (int i = 0; i < (1 << 16); i++)
	{
		if (state.DataInfo[i] != nullptr)
		{
			FDataInfo & dataInfo = *state.DataInfo[i];
			dataInfo.LastFrameRead = 0;
			dataInfo.Reads.clear();
			dataInfo.LastFrameWritten = 0;
			dataInfo.Writes.clear();
		}

		state.LastWriter[i] = 0;
	}
}



FLabelInfo* AddLabel(FCodeAnalysisState &state, uint16_t address,const char *name,LabelType type)
{
	FLabelInfo *pLabel = new FLabelInfo;
	pLabel->Name = name;
	pLabel->LabelType = type;
	pLabel->Address = address;
	pLabel->ByteSize = 1;
	pLabel->Global = type == LabelType::Function;
	state.Labels[address] = pLabel;
	return pLabel;
}

void GenerateGlobalInfo(FCodeAnalysisState &state)
{
	state.GlobalDataItems.clear();
	state.GlobalFunctions.clear();

	for (int i = 0; i < (1 << 16); i++)
	{
		FLabelInfo *pLabel = state.Labels[i];
		
		if (pLabel != nullptr)
		{
			if (pLabel->LabelType == LabelType::Data && pLabel->Global)
				state.GlobalDataItems.push_back(pLabel);
			if (pLabel->LabelType == LabelType::Function)
				state.GlobalFunctions.push_back(pLabel);
		}
		
	}
}




void InitialiseCodeAnalysis(FCodeAnalysisState &state, ICPUInterface* pCPUInterface)
{
	for (int i = 0; i < (1 << 16); i++)	// loop across address range
	{
		delete state.Labels[i];
		state.Labels[i] = nullptr;

		if (state.CodeInfo[i] != nullptr)
		{
			FCodeInfo *pCodeInfo = state.CodeInfo[i];
			const int codeSize = pCodeInfo->ByteSize;
			for (int off = 0; off < codeSize; off++)
				state.CodeInfo[i + off] = nullptr;
			delete pCodeInfo;
		}

		delete state.DataInfo[i];

		// set up data entry for address
		FDataInfo *pDataInfo = new FDataInfo;
		pDataInfo->Address = (uint16_t)i;
		pDataInfo->ByteSize = 1;
		pDataInfo->DataType = DataType::Byte;
		state.DataInfo[i] = pDataInfo;
	}

	state.CursorItemIndex = -1;
	state.pCursorItem = nullptr;
	
	state.CPUInterface = pCPUInterface;
	uint16_t initialPC = pCPUInterface->GetPC();// z80_pc(&state.pSpeccy->CurrentState.cpu);
	pCPUInterface->InsertROMLabels(state);
	pCPUInterface->InsertSystemLabels(state);
	RunStaticCodeAnalysis(state, initialPC);

	// Key Config
	state.KeyConfig[(int)Key::SetItemData] = 'D';
	state.KeyConfig[(int)Key::SetItemText] = 'T';
	state.KeyConfig[(int)Key::SetItemCode] = 'C';
	state.KeyConfig[(int)Key::AddLabel] = 'L';
	state.KeyConfig[(int)Key::Rename] = 'R';
	state.KeyConfig[(int)Key::Comment] = 0xBF;
}

// Command Processing
void DoCommand(FCodeAnalysisState &state, FCommand *pCommand)
{
	state.CommandStack.push_back(pCommand);
	pCommand->Do(state);
}

void Undo(FCodeAnalysisState &state)
{
	if (state.CommandStack.empty() == false)
	{
		state.CommandStack.back()->Undo(state);
		state.CommandStack.pop_back();
	}
}

class FSetItemDataCommand : public FCommand
{
public:
	FSetItemDataCommand(FItem *_pItem) :pItem(_pItem) {}

	virtual void Do(FCodeAnalysisState &state) override
	{

		if (pItem->Type == ItemType::Data)
		{
			FDataInfo *pDataItem = static_cast<FDataInfo *>(pItem);

			oldDataType = pDataItem->DataType;
			oldDataSize = pDataItem->ByteSize;

			if (pDataItem->DataType == DataType::Byte)
			{
				pDataItem->DataType = DataType::Word;
				pDataItem->ByteSize = 2;
				state.bCodeAnalysisDataDirty = true;
			}
			else if (pDataItem->DataType == DataType::Word)
			{
				pDataItem->DataType = DataType::Byte;
				pDataItem->ByteSize = 1;
				state.bCodeAnalysisDataDirty = true;
			}
			else if ( pDataItem->DataType == DataType::Text )
			{
				pDataItem->DataType = DataType::Byte;
				pDataItem->ByteSize = 1;
				state.bCodeAnalysisDataDirty = true;
			}
		}
		else if (pItem->Type == ItemType::Code)
		{
			FCodeInfo *pCodeItem = static_cast<FCodeInfo *>(pItem);
			if (pCodeItem->bDisabled == false)
			{
				pCodeItem->bDisabled = true;
				state.bCodeAnalysisDataDirty = true;

				if (state.Labels[pItem->Address] != nullptr)
					state.Labels[pItem->Address]->LabelType = LabelType::Data;
			}
		}
	}
	virtual void Undo(FCodeAnalysisState &state) override
	{
		FDataInfo *pDataItem = static_cast<FDataInfo *>(pItem);
		pDataItem->DataType = oldDataType;
		pDataItem->ByteSize = oldDataSize;
	}

	FItem *	pItem;

	DataType	oldDataType;
	uint16_t	oldDataSize;
};

void SetItemCode(FCodeAnalysisState &state, FItem *pItem)
{
	if(state.CodeInfo[pItem->Address] !=nullptr && state.CodeInfo[pItem->Address]->bDisabled == true)
	{
		state.CodeInfo[pItem->Address]->bDisabled = false;
	}
	else
	{
		RunStaticCodeAnalysis(state, pItem->Address);
		UpdateCodeInfoForAddress(state, pItem->Address);
	}
	state.bCodeAnalysisDataDirty = true;
}

void SetItemData(FCodeAnalysisState &state, FItem *pItem)
{
	DoCommand(state, new FSetItemDataCommand(pItem));
}

void SetItemText(FCodeAnalysisState &state, FItem *pItem)
{
	if (pItem->Type == ItemType::Data)
	{
		FDataInfo *pDataItem = static_cast<FDataInfo *>(pItem);
		if (pDataItem->DataType == DataType::Byte)
		{
			// set to ascii
			pDataItem->ByteSize = 0;	// reset byte counter

			uint16_t charAddr = pDataItem->Address;
			while (state.DataInfo[charAddr] != nullptr && state.DataInfo[charAddr]->DataType == DataType::Byte)
			{
				const uint8_t val = state.CPUInterface->ReadByte(charAddr);
				if (val == 0 || val > 0x80)
					break;
				pDataItem->ByteSize++;
				charAddr++;
			}

			// did the operation fail? -revert to byte
			if (pDataItem->ByteSize == 0)
			{
				pDataItem->DataType = DataType::Byte;
				pDataItem->ByteSize = 1;
			}
			else
			{
				pDataItem->DataType = DataType::Text;
				state.bCodeAnalysisDataDirty = true;
			}
		}
	}
}
void AddLabelAtAddress(FCodeAnalysisState &state, uint16_t address)
{
	if (state.Labels[address] == nullptr)
	{
		LabelType labelType = LabelType::Data;
		if (state.CodeInfo[address] != nullptr && state.CodeInfo[address]->bDisabled == false)
			labelType = LabelType::Code;

		GenerateLabelForAddress(state, address, labelType);
		state.bCodeAnalysisDataDirty = true;
	}
}

void RemoveLabelAtAddress(FCodeAnalysisState &state, uint16_t address)
{
	if (state.Labels[address] != nullptr)
	{
		delete state.Labels[address];
		state.Labels[address] = nullptr;
		state.bCodeAnalysisDataDirty = true;
	}
}

void SetLabelName(FCodeAnalysisState &state, FLabelInfo *pLabel, const char *pText)
{
	pLabel->Name = pText;
}

void SetItemCommentText(FCodeAnalysisState &state, FItem *pItem, const char *pText)
{
	pItem->Comment = pText;
}

// text generation

std::string GenerateAddressLabelString(FCodeAnalysisState &state, uint16_t addr)
{
	int labelOffset = 0;
	const char *pLabelString = nullptr;
	std::string labelStr;
	
	for (int addrVal = addr; addrVal >= 0; addrVal--)
	{
		if (state.Labels[addrVal] != nullptr)
		{
			labelStr = "[" + state.Labels[addrVal]->Name;
			break;
		}

		labelOffset++;
	}

	if (labelStr.empty() == false)
	{
		if (labelOffset > 0)	// add offset string
		{
			char offsetString[16];
			sprintf(offsetString, " + %d]", labelOffset);
			labelStr += offsetString;
		}
		else
		{
			labelStr += "]";
		}
	}

	return labelStr;
}

bool OutputCodeAnalysisToTextFile(FCodeAnalysisState &state, const char *pTextFileName,uint16_t startAddr,uint16_t endAddr)
{
	FILE *fp = fopen(pTextFileName, "wt");
	if (fp == NULL)
		return false;
	
	for(FItem* pItem : state.ItemList)
	{
		if (pItem->Address < startAddr || pItem->Address > endAddr)
			continue;
		
		switch (pItem->Type)
		{
		case ItemType::Label:
			{
				const FLabelInfo *pLabelInfo = static_cast<FLabelInfo *>(pItem);
				fprintf(fp, "%s:", pLabelInfo->Name.c_str());
			}
			break;
		case ItemType::Code:
			{
				const FCodeInfo *pCodeInfo = static_cast<FCodeInfo *>(pItem);
				fprintf(fp, "\t%s", pCodeInfo->Text.c_str());

				if (pCodeInfo->JumpAddress != 0)
				{
					const std::string labelStr = GenerateAddressLabelString(state, pCodeInfo->JumpAddress);
					if(labelStr.empty() == false)
						fprintf(fp,"\t;%s", labelStr.c_str());
					
				}
				else if (pCodeInfo->PointerAddress != 0)
				{
					const std::string labelStr = GenerateAddressLabelString(state, pCodeInfo->PointerAddress);
					if (labelStr.empty() == false)
						fprintf(fp, "\t;%s", labelStr.c_str());
				}
			}
			
			break;
		case ItemType::Data:
			{
				const FDataInfo *pDataInfo = static_cast<FDataInfo *>(pItem);

				fprintf(fp, "\t");
				switch (pDataInfo->DataType)
				{
				case DataType::Byte:
				{
					const uint8_t val = state.CPUInterface->ReadByte(pDataInfo->Address);
					fprintf(fp,"db %02Xh", val);
				}
				break;

				case DataType::Word:
				{
					const uint16_t val = state.CPUInterface->ReadByte(pDataInfo->Address) | (state.CPUInterface->ReadByte(pDataInfo->Address + 1) << 8);
					fprintf(fp, "dw %04Xh", val);
				}
				break;

				case DataType::Text:
				{
					std::string textString;
					for (int i = 0; i < pDataInfo->ByteSize; i++)
					{
						const char ch = state.CPUInterface->ReadByte(pDataInfo->Address + i);
						if (ch == '\n')
							textString += "<cr>";
						else
							textString += ch;
					}
					fprintf(fp, "ascii '%s'", textString.c_str());
				}
				break;

				case DataType::Graphics:
				case DataType::Blob:
				default:
					fprintf(fp, "%d Bytes", pDataInfo->ByteSize);
					break;
				}
			}
			break;
		}

		// put comment on the end
		if(pItem->Comment.empty() == false)
		fprintf(fp, "\t;%s", pItem->Comment.c_str());
		fprintf(fp, "\n");
	}

	fclose(fp);
	return true;
}