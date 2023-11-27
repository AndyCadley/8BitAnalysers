#include "Disassembler.h"

#include "CodeAnalyser.h"


void FAnalysisDasmState::OutputU8(uint8_t val, dasm_output_t outputCallback) 
{
	if (outputCallback != nullptr)
	{
		ENumberDisplayMode dispMode = GetNumberDisplayMode();

		if (pCodeInfoItem->OperandType == EOperandType::Pointer || pCodeInfoItem->OperandType == EOperandType::JumpAddress)
		{
			char pointerMarkup[16];
			//snprintf(pointerMarkup,16,"#ADDR:0x%04X#", val);
			strcpy(pointerMarkup, "#OPERAND_ADDR#");
			for (int i = 0; i < strlen(pointerMarkup); i++)
				outputCallback(pointerMarkup[i], this);
			return;
		}

		if (pCodeInfoItem->OperandType == EOperandType::Decimal)
			dispMode = ENumberDisplayMode::Decimal;
		if (pCodeInfoItem->OperandType == EOperandType::Hex)
			dispMode = ENumberDisplayMode::HexAitch;
		if (pCodeInfoItem->OperandType == EOperandType::Binary)
			dispMode = ENumberDisplayMode::Binary;

		const char* outStr = NumStr(val, dispMode);
		for (int i = 0; i < strlen(outStr); i++)
			outputCallback(outStr[i], this);
	}
}

void FAnalysisDasmState::OutputU16(uint16_t val, dasm_output_t outputCallback) 
{
	if (outputCallback)
	{
		ENumberDisplayMode dispMode = GetNumberDisplayMode();

		if (pCodeInfoItem->OperandType == EOperandType::Pointer || pCodeInfoItem->OperandType == EOperandType::JumpAddress)
		{
			char pointerMarkup[16];
			//snprintf(pointerMarkup,16,"#ADDR:0x%04X#", val);
			strcpy(pointerMarkup, "#OPERAND_ADDR#");
			for (int i = 0; i < strlen(pointerMarkup); i++)
				outputCallback(pointerMarkup[i], this);
			return;
		}

		if (pCodeInfoItem->OperandType == EOperandType::Decimal)
			dispMode = ENumberDisplayMode::Decimal;
		else if (pCodeInfoItem->OperandType == EOperandType::Hex)
			dispMode = ENumberDisplayMode::HexAitch;
		else if (pCodeInfoItem->OperandType == EOperandType::Binary)
			dispMode = ENumberDisplayMode::Binary;

		const char* outStr = NumStr(val, dispMode);
		for (int i = 0; i < strlen(outStr); i++)
			outputCallback(outStr[i], this);
	}
}

void FAnalysisDasmState::OutputD8(int8_t val, dasm_output_t outputCallback) 
{
	if (outputCallback)
	{
		if (val < 0)
		{
			outputCallback('-', this);
			val = -val;
		}
		else
		{
			outputCallback('+', this);
		}
		const char* outStr = NumStr((uint8_t)val);
		for (int i = 0; i < strlen(outStr); i++)
			outputCallback(outStr[i], this);
	}
}

FAnalysisDasmState testA;

// disassembler callback to fetch the next instruction byte 
uint8_t AnalysisDasmInputCB(void* pUserData)
{
	FAnalysisDasmState* pDasmState = (FAnalysisDasmState*)pUserData;

	return pDasmState->CodeAnalysisState->ReadByte(pDasmState->CurrentAddress++);
}

// disassembler callback to output a character 
void AnalysisOutputCB(char c, void* pUserData)
{
	FAnalysisDasmState* pDasmState = (FAnalysisDasmState*)pUserData;

	// add character to string
	pDasmState->Text += c;
}

// For Aseembler exporter

void FExportDasmState::OutputU8(uint8_t val, dasm_output_t outputCallback) 
{
	if (outputCallback != nullptr)
	{
		ENumberDisplayMode dispMode = GetNumberDisplayMode();

		if (pCodeInfoItem->OperandType == EOperandType::Decimal)
			dispMode = ENumberDisplayMode::Decimal;
		if (pCodeInfoItem->OperandType == EOperandType::Hex)
			dispMode = HexDisplayMode;
		if (pCodeInfoItem->OperandType == EOperandType::Binary)
			dispMode = ENumberDisplayMode::Binary;

		const char* outStr = NumStr(val, dispMode);
		for (int i = 0; i < strlen(outStr); i++)
			outputCallback(outStr[i], this);
	}
}

void FExportDasmState::OutputU16(uint16_t val, dasm_output_t outputCallback) 
{
	if (outputCallback)
	{
		const bool bOperandIsAddress = (pCodeInfoItem->OperandType == EOperandType::JumpAddress || pCodeInfoItem->OperandType == EOperandType::Pointer);
		const FLabelInfo* pLabel = bOperandIsAddress ? CodeAnalysisState->GetLabelForPhysicalAddress(val) : nullptr;
		if (pLabel != nullptr)
		{
			std::string labelName(pLabel->GetName());
			for (int i = 0; i < labelName.size(); i++)
			{
				outputCallback(labelName[i], this);
			}
		}
		else
		{
			ENumberDisplayMode dispMode = GetNumberDisplayMode();

			if (pCodeInfoItem->OperandType == EOperandType::Decimal)
				dispMode = ENumberDisplayMode::Decimal;
			if (pCodeInfoItem->OperandType == EOperandType::Hex)
				dispMode = HexDisplayMode;
			if (pCodeInfoItem->OperandType == EOperandType::Binary)
				dispMode = ENumberDisplayMode::Binary;

			const char* outStr = NumStr(val, dispMode);
			for (int i = 0; i < strlen(outStr); i++)
				outputCallback(outStr[i], this);
		}
	}
}

void FExportDasmState::OutputD8(int8_t val, dasm_output_t outputCallback) 
{
	if (outputCallback)
	{
		if (val < 0)
		{
			outputCallback('-', this);
			val = -val;
		}
		else
		{
			outputCallback('+', this);
		}
		const char* outStr = NumStr((uint8_t)val);
		for (int i = 0; i < strlen(outStr); i++)
			outputCallback(outStr[i], this);
	}
}

FExportDasmState testB;

/* disassembler callback to fetch the next instruction byte */
uint8_t ExportDasmInputCB(void* pUserData)
{
	FExportDasmState* pDasmState = (FExportDasmState*)pUserData;

	return pDasmState->CodeAnalysisState->CPUInterface->ReadByte(pDasmState->CurrentAddress++);
}

/* disassembler callback to output a character */
void ExportOutputCB(char c, void* pUserData)
{
	FExportDasmState* pDasmState = (FExportDasmState*)pUserData;

	// add character to string
	pDasmState->Text += c;
}


static IDasmNumberOutput* g_pNumberOutputObj = nullptr;
static IDasmNumberOutput* GetNumberOutput()
{
	return g_pNumberOutputObj;
}

void SetNumberOutput(IDasmNumberOutput* pNumberOutputObj)
{
	g_pNumberOutputObj = pNumberOutputObj;
}

// output an unsigned 8-bit value as hex string 
void DasmOutputU8(uint8_t val, dasm_output_t out_cb, void* user_data)
{
	IDasmNumberOutput* pNumberOutput = GetNumberOutput();
	if (pNumberOutput)
		pNumberOutput->OutputU8(val, out_cb);

}

// output an unsigned 16-bit value as hex string 
void DasmOutputU16(uint16_t val, dasm_output_t out_cb, void* user_data)
{
	IDasmNumberOutput* pNumberOutput = GetNumberOutput();
	if (pNumberOutput)
		pNumberOutput->OutputU16(val, out_cb);
}

// output a signed 8-bit offset as hex string 
void DasmOutputD8(int8_t val, dasm_output_t out_cb, void* user_data)
{
	IDasmNumberOutput* pNumberOutput = GetNumberOutput();
	if (pNumberOutput)
		pNumberOutput->OutputD8(val, out_cb);
}