#include "JsonExport.h"
#include "CodeAnalyser/CodeAnalyser.h"
#include "../SpectrumConstants.h"
#include "../SpectrumEmu.h"

#include <json.hpp>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <Util/GraphicsView.h>
#include <Debug/DebugLog.h>

using json = nlohmann::json;

// We want to eventually move to using Json as it will allow merging 
void WriteAddressRangeToJson(FCodeAnalysisState& state, int startAddress, int endAddress, json& jsonDoc);
void WritePageToJson(const FCodeAnalysisPage& page, json& jsonDoc);
void ReadPageFromJson(FCodeAnalysisPage& page, const json& jsonDoc);


// Analysis State Load/Save 
// TODO: move to shared area

void WritePageState(const FCodeAnalysisPage& page, FILE *fp)
{
}

bool ExportAnalysisState(FCodeAnalysisState& state, const char* pAnalysisBinFile)
{
	FILE* fp = fopen(pAnalysisBinFile, "wb");
	if (fp == nullptr)
		return false;

	const auto& banks = state.GetBanks();
	int pagesWritten = 0;

	// iterate through all registered banks
	for (int bankNo = 0; bankNo < banks.size(); bankNo++)
	{
		const FCodeAnalysisBank& bank = banks[bankNo];
		if (bank.bReadOnly)	// skip read only banks - ROM
			continue;

		for (int pageNo = 0; pageNo < bank.NoPages; pageNo++)
		{
			const FCodeAnalysisPage& page = bank.Pages[pageNo];
			if (page.bUsed)
			{
				WritePageState(page, fp);
				pagesWritten++;
			}
		}
	}

	fclose(fp);
	return true;
}

bool ImportAnalysisState(FCodeAnalysisState& state, const char* pAnalysisBinFile)
{
	FILE* fp = fopen(pAnalysisBinFile, "rb");
	if (fp == nullptr)
		return false;

	fclose(fp);
	return true;
}

bool ExportROMAnalysisJson(FCodeAnalysisState& state, const char* pJsonFileName)
{
	json jsonROMData;

	WriteAddressRangeToJson(state, 0, 0x3fff, jsonROMData);

	std::ofstream outFileStream(pJsonFileName);
	if (outFileStream.is_open())
	{
		outFileStream << std::setw(4) << jsonROMData << std::endl;
		return true;
	}

	return false;
}

bool ExportGameAnalysisJson(FSpectrumEmu* pSpectrumEmu, const char* pJsonFileName)
{
	FCodeAnalysisState& state = pSpectrumEmu->CodeAnalysis;
	json jsonGameData;

	int pagesWritten = 0;
	const auto& banks = state.GetBanks();

	// iterate through all registered banks
	for (int bankNo = 0; bankNo < banks.size(); bankNo++)
	{
		const FCodeAnalysisBank& bank = banks[bankNo];
		if (bank.bReadOnly)	// skip read only banks - ROM
			continue;

		for (int pageNo = 0; pageNo < bank.NoPages; pageNo++)
		{
			const FCodeAnalysisPage& page = bank.Pages[pageNo];
			if (page.bUsed)
			{
				json pageData;

				WritePageToJson(page, pageData);
				jsonGameData["Pages"].push_back(pageData);
				pagesWritten++;
			}
		}
	}
	LOGINFO("%d pages written", pagesWritten);

	// Write watches
	for (const auto& watch : state.GetWatches())
	{
		jsonGameData["Watches"].push_back(watch.Address);
	}

	// Spectrum Specific

	// Write character sets
	for (int i = 0; i < GetNoCharacterSets(); i++)
	{
		const FCharacterSet* pCharSet = GetCharacterSetFromIndex(i);
		json jsonCharacterSet;

		jsonCharacterSet["Address"] = pCharSet->Params.Address;
		jsonCharacterSet["AttribsAddress"] = pCharSet->Params.AttribsAddress;
		jsonCharacterSet["MaskInfo"] = pCharSet->Params.MaskInfo;
		jsonCharacterSet["ColourInfo"] = pCharSet->Params.ColourInfo;
		jsonCharacterSet["Dynamic"] = pCharSet->Params.bDynamic;

		jsonGameData["CharacterSets"].push_back(jsonCharacterSet);
	}

	// Write character maps
	for (int i = 0; i < GetNoCharacterMaps(); i++)
	{
		const FCharacterMap* pCharMap = GetCharacterMapFromIndex(i);
		json jsonCharacterMap;

		jsonCharacterMap["Address"] = pCharMap->Params.Address;
		jsonCharacterMap["Width"] = pCharMap->Params.Width;
		jsonCharacterMap["Height"] = pCharMap->Params.Height;
		jsonCharacterMap["CharacterSet"] = pCharMap->Params.CharacterSet;
		jsonCharacterMap["IgnoreCharacter"] = pCharMap->Params.IgnoreCharacter;

		jsonGameData["CharacterMaps"].push_back(jsonCharacterMap);
	}

	// Write file out
	std::ofstream outFileStream(pJsonFileName);
	if (outFileStream.is_open())
	{
		outFileStream << std::setw(4) << jsonGameData << std::endl;
		return true;
	}

	return false;
}

bool WriteDataInfoToJson(uint16_t addr,const FDataInfo* pDataInfo, json& jsonDoc, int addressOverride = -1)
{
	json dataInfoJson;

	assert(pDataInfo != nullptr);
	if (pDataInfo->DataType != EDataType::Byte)
		dataInfoJson["DataType"] = (int)pDataInfo->DataType;
	if (pDataInfo->OperandType != EOperandType::Unknown)
		dataInfoJson["OperandType"] = pDataInfo->OperandType;
	if (pDataInfo->ByteSize != 1)
		dataInfoJson["ByteSize"] = pDataInfo->ByteSize;
	if (pDataInfo->Flags != 0)
		dataInfoJson["Flags"] = pDataInfo->Flags;
	if (pDataInfo->Comment.empty() == false)
		dataInfoJson["Comment"] = pDataInfo->Comment;

	// These have moved to a binary file
	//for (const auto& read : pDataInfo->Reads.GetReferences())
	//	dataInfoJson["Reads"].push_back(read.InstructionAddress);

	//for (const auto& write : pDataInfo->Writes.GetReferences())
	//	dataInfoJson["Writes"].push_back(write.InstructionAddress);

	//if (pDataInfo->LastWriter != 0)
	//	dataInfoJson["LastWriter"] = pDataInfo->LastWriter;

	// Charmap specific
	if (pDataInfo->DataType == EDataType::CharacterMap)
	{
		dataInfoJson["CharSetAddress"] = pDataInfo->CharSetAddress;
		dataInfoJson["EmptyCharNo"] = pDataInfo->EmptyCharNo;
	}

	if (dataInfoJson.size() != 0)	// only write it if it deviates from the normal
	{
		dataInfoJson["Address"] = addressOverride == -1 ? addr : addressOverride;
		jsonDoc["DataInfo"].push_back(dataInfoJson);
		return true;
	}

	return false;
}

void WriteCodeInfoToJson(uint16_t addr,const FCodeInfo* pCodeInfoItem, json& jsonDoc, int addressOverride = -1)
{
	json codeInfoJson;
	codeInfoJson["Address"] = addressOverride == -1 ? addr : addressOverride;
	codeInfoJson["ByteSize"] = pCodeInfoItem->ByteSize;
	if (pCodeInfoItem->bSelfModifyingCode)
		codeInfoJson["SMC"] = true;
	if (pCodeInfoItem->OperandType != EOperandType::Unknown)
		codeInfoJson["OperandType"] = (int)pCodeInfoItem->OperandType;
	if (pCodeInfoItem->Flags != 0)
		codeInfoJson["Flags"] = pCodeInfoItem->Flags;
	if (pCodeInfoItem->Comment.empty() == false)
		codeInfoJson["Comment"] = pCodeInfoItem->Comment;

	jsonDoc["CodeInfo"].push_back(codeInfoJson);
}

void WriteLabelInfoToJson(uint16_t addr, const FLabelInfo* pLabelInfo, json& jsonDoc, int addressOverride = -1)
{
	json labelInfoJson;
	labelInfoJson["Address"] = addressOverride == -1 ? addr : addressOverride;
	labelInfoJson["Name"] = pLabelInfo->Name;
	if (pLabelInfo->Global)
		labelInfoJson["Global"] = pLabelInfo->Global;
	labelInfoJson["LabelType"] = pLabelInfo->LabelType;
	if (pLabelInfo->Comment.empty() == false)
		labelInfoJson["Comment"] = pLabelInfo->Comment;

	// These have moved to a binary file
	//for (const auto& reference : pLabelInfo->References.GetReferences())
	//	labelInfoJson["References"].push_back(reference.InstructionAddress);

	jsonDoc["LabelInfo"].push_back(labelInfoJson);
}

void WriteCommentBlockToJson(uint16_t addr, const FCommentBlock* pCommentBlock, json& jsonDoc, int addressOverride = -1)
{
	if (pCommentBlock->Comment.empty())
		return;

	json commentBlockJson;
	commentBlockJson["Address"] = addressOverride == -1 ? addr: addressOverride;
	commentBlockJson["Comment"] = pCommentBlock->Comment;

	jsonDoc["CommentBlocks"].push_back(commentBlockJson);
}

void WriteAddressRangeToJson(FCodeAnalysisState& state, int startAddress,int endAddress, json& jsonDoc)
{
	int address = startAddress;

	// info on last writer
	// TODO: move to binary
	//jsonDoc["LastWriterStart"] = startAddress;
	//for (int addr = startAddress; addr <= endAddress; addr++)
	//	jsonDoc["LastWriter"].push_back(state.GetLastWriterForAddress(addr));
	
	while(address <= endAddress)
	{
		FCommentBlock* pCommentBlock = state.GetCommentBlockForAddress(address);
		if (pCommentBlock != nullptr)
			WriteCommentBlockToJson(address,pCommentBlock, jsonDoc);

		FLabelInfo* pLabelInfo = state.GetLabelForAddress(address);
		if (pLabelInfo)
			WriteLabelInfoToJson(address, pLabelInfo, jsonDoc);

		FCodeInfo* pCodeInfoItem = state.GetCodeInfoForAddress(address);
		if (pCodeInfoItem)	// only write code items for first byte of the instruction
		{
			WriteCodeInfoToJson(address, pCodeInfoItem, jsonDoc);
			
			if (pCodeInfoItem->bSelfModifyingCode == false)	// this is so that we can write info on SMC accesses
				address += pCodeInfoItem->ByteSize;
		}
		
		if(pCodeInfoItem == nullptr || pCodeInfoItem->bSelfModifyingCode)
		{
			FDataInfo* pDataInfo = state.GetReadDataInfoForAddress(address);
			WriteDataInfoToJson(address, pDataInfo, jsonDoc);
			address += pDataInfo->ByteSize;
		}
	}
}


FCommentBlock* CreateCommentBlockFromJson(const json& commentBlockJson)
{
	FCommentBlock* pCommentBlock = FCommentBlock::Allocate();
	//pCommentBlock->Address = commentBlockJson["Address"];
	pCommentBlock->Comment = commentBlockJson["Comment"];
	return pCommentBlock;
}

FCodeInfo* CreateCodeInfoFromJson(const json& codeInfoJson)
{
	FCodeInfo* pCodeInfo = FCodeInfo::Allocate();
	pCodeInfo->ByteSize = codeInfoJson["ByteSize"];

	if (codeInfoJson.contains("SMC"))
		pCodeInfo->bSelfModifyingCode = codeInfoJson["SMC"];

	if (codeInfoJson.contains("OperandType"))
		pCodeInfo->OperandType = codeInfoJson["OperandType"];

	if (codeInfoJson.contains("Flags"))
		pCodeInfo->Flags = codeInfoJson["Flags"];

	if (codeInfoJson.contains("Comment"))
		pCodeInfo->Comment = codeInfoJson["Comment"];

	return pCodeInfo;
}

FLabelInfo* CreateLabelInfoFromJson(const json& labelInfoJson)
{
	FLabelInfo* pLabelInfo = FLabelInfo::Allocate();

	pLabelInfo->Name = labelInfoJson["Name"];
	if (labelInfoJson.contains("Global"))
		pLabelInfo->Global = true;

	if (labelInfoJson.contains("LabelType"))
		pLabelInfo->LabelType = (ELabelType)(int)labelInfoJson["LabelType"];
	if (labelInfoJson.contains("Comment"))
		pLabelInfo->Comment = labelInfoJson["Comment"];

	// Moved to binary file
	/*if (labelInfoJson.contains("References"))
	{
		for (const auto& reference : labelInfoJson["References"])
		{
			pLabelInfo->References.RegisterAccess(reference);
		}
	}*/

	return pLabelInfo;
}

void LoadDataInfoFromJson(FDataInfo* pDataInfo, const json & dataInfoJson)
{
	if (dataInfoJson.contains("DataType"))
		pDataInfo->DataType = (EDataType)(int)dataInfoJson["DataType"];
	if (dataInfoJson.contains("OperandType"))
		pDataInfo->OperandType = (EOperandType)(int)dataInfoJson["OperandType"];
	if (dataInfoJson.contains("ByteSize"))
		pDataInfo->ByteSize = dataInfoJson["ByteSize"];
	if (dataInfoJson.contains("Flags"))
		pDataInfo->Flags = dataInfoJson["Flags"];
	if (dataInfoJson.contains("Comment"))
		pDataInfo->Comment = dataInfoJson["Comment"];

	// Moved to binary file
	/*
	if (dataInfoJson.contains("Reads"))
	{
		for (const auto& read : dataInfoJson["Reads"])
		{
			pDataInfo->Reads.RegisterAccess(read);
		}
	}
	if (dataInfoJson.contains("Writes"))
	{
		for (const auto& write : dataInfoJson["Writes"])
		{
			pDataInfo->Writes.RegisterAccess(write);
		}
	}

	if (dataInfoJson.contains("LastWriter"))
		pDataInfo->LastWriter = dataInfoJson["LastWriter"];
*/
	// Charmap specific
	if (pDataInfo->DataType == EDataType::CharacterMap)
	{
		if(dataInfoJson.contains("CharSetAddress"))
			pDataInfo->CharSetAddress = dataInfoJson["CharSetAddress"];
		if (dataInfoJson.contains("EmptyCharNo"))
			pDataInfo->EmptyCharNo = dataInfoJson["EmptyCharNo"];
	}
}


bool ImportAnalysisJson(FSpectrumEmu* pSpectrumEmu,  const char* pJsonFileName)
{
	FCodeAnalysisState& state = pSpectrumEmu->CodeAnalysis;

	std::ifstream inFileStream(pJsonFileName);
	if (inFileStream.is_open() == false)
		return false;

	json jsonGameData;

	inFileStream >> jsonGameData;
	inFileStream.close();

	if (jsonGameData.contains("Pages"))
	{
		for (const auto& pageJson : jsonGameData["Pages"])
		{
			const int pageId = pageJson["PageId"];
			FCodeAnalysisPage* pPage = state.GetPage(pageId);
			if (pPage != nullptr)
			{
				ReadPageFromJson(*pPage, pageJson);
			}
		}
	}

	// info on last writer
	if (jsonGameData.contains("LastWriterStart"))
	{
		const int lwStart = jsonGameData["LastWriterStart"];

		const json& lastWriterArray = jsonGameData["LastWriter"];
		const int noWriters = (int)lastWriterArray.size();
		for (int i = 0;i<noWriters;i++)
			state.SetLastWriterForAddress(lwStart + i, state.AddressRefFromPhysicalAddress(lastWriterArray[i]));
	}

	if (jsonGameData.contains("CommentBlocks"))
	{
		for (const auto& commentBlockJson : jsonGameData["CommentBlocks"])
		{
			const uint16_t addr = commentBlockJson["Address"];
			FCommentBlock* pCommentBlock = CreateCommentBlockFromJson(commentBlockJson);
			state.SetCommentBlockForAddress(addr, pCommentBlock);
		}
	}

	if (jsonGameData.contains("CodeInfo"))
	{
		for (const auto codeInfoJson : jsonGameData["CodeInfo"])
		{
			const uint16_t addr = codeInfoJson["Address"];
			FCodeInfo* pCodeInfo = CreateCodeInfoFromJson(codeInfoJson);
			state.SetCodeInfoForAddress(addr, pCodeInfo);

			// set operand data items
			for (int codeByte = 1; codeByte < pCodeInfo->ByteSize; codeByte++)	
			{
				FDataInfo* pDataInfo = state.GetReadDataInfoForAddress(addr + codeByte);
				pDataInfo->DataType = EDataType::InstructionOperand;
				pDataInfo->ByteSize = 1;
				pDataInfo->InstructionAddress = addr;
			}
		}
	}

	if (jsonGameData.contains("LabelInfo"))
	{
		for (const auto labelInfoJson : jsonGameData["LabelInfo"])
		{
			const uint16_t addr = labelInfoJson["Address"];
			FLabelInfo* pLabelInfo = CreateLabelInfoFromJson(labelInfoJson);
			state.SetLabelForAddress(addr,pLabelInfo);
		}
	}

	if (jsonGameData.contains("DataInfo"))
	{
		for (const auto dataInfoJson : jsonGameData["DataInfo"])
		{
			const int address = dataInfoJson["Address"];
			FDataInfo* pDataInfo = state.GetReadDataInfoForAddress(address);
			LoadDataInfoFromJson(pDataInfo, dataInfoJson);
		}
	}

	if (jsonGameData.contains("Watches"))
	{
		for (const auto& watchAddress : jsonGameData["Watches"])
		{
			if(watchAddress.is_number_integer())
				state.AddWatch({ state.GetBankFromAddress(watchAddress), watchAddress });	
			else if(watchAddress.is_object())
				state.AddWatch({ watchAddress["Bank"], watchAddress["Address"]});	
		}
	}

	// Spectrum specific

	if (jsonGameData.contains("CharacterSets"))
	{
		for (const auto& charSet : jsonGameData["CharacterSets"])
		{
			FCharSetCreateParams params;
			params.Address = charSet["Address"];
			params.AttribsAddress = charSet["AttribsAddress"];
			params.MaskInfo = charSet["MaskInfo"];
			params.ColourInfo = charSet["ColourInfo"];
			params.bDynamic = charSet["Dynamic"];
			params.ColourLUT = state.Config.CharacterColourLUT;
			CreateCharacterSetAt(state, params);
		}
	}

	if (jsonGameData.contains("CharacterMaps"))
	{
		for (const auto& charMap : jsonGameData["CharacterMaps"])
		{
			FCharMapCreateParams params;
			params.Address = charMap["Address"];
			params.Width = charMap["Width"];
			params.Height = charMap["Height"];
			params.CharacterSet = charMap["CharacterSet"];
			params.IgnoreCharacter = charMap["IgnoreCharacter"];
			CreateCharacterMap(state, params);
		}
	}

	return true;
}

// write a 1K page to Json
// the plan is to move to this so we can support 128K games
void WritePageToJson(const FCodeAnalysisPage& page, json& jsonDoc)
{
	jsonDoc["PageId"] = page.PageId;

	int pageAddr = 0;

	while(pageAddr < FCodeAnalysisPage::kPageSize)
	{		
		const FCommentBlock* pCommentBlock = page.CommentBlocks[pageAddr];
		if (pCommentBlock != nullptr)
			WriteCommentBlockToJson(pageAddr, pCommentBlock, jsonDoc);

		const FLabelInfo* pLabelInfo = page.Labels[pageAddr];
		if (pLabelInfo)
			WriteLabelInfoToJson(pageAddr, pLabelInfo, jsonDoc);

		const FCodeInfo* pCodeInfoItem = page.CodeInfo[pageAddr];
		if (pCodeInfoItem)
		{
			WriteCodeInfoToJson(pageAddr, pCodeInfoItem, jsonDoc);
			if (pCodeInfoItem->bSelfModifyingCode == false)
			{
				pageAddr += pCodeInfoItem->ByteSize;
			}
		}

		// we do want data info for SMC operands
		if (pCodeInfoItem == nullptr || pCodeInfoItem->bSelfModifyingCode == true)
		{
			const FDataInfo* pDataInfo = &page.DataInfo[pageAddr];
			WriteDataInfoToJson(pageAddr, pDataInfo, jsonDoc);
			pageAddr += pDataInfo->ByteSize;
		}

	}
}


void ReadPageFromJson(FCodeAnalysisPage& page, const json& jsonDoc)
{
	if (jsonDoc.contains("CommentBlocks"))
	{
		for (const auto commentBlockJson : jsonDoc["CommentBlocks"])
		{
			const uint16_t pageAddr = commentBlockJson["Address"];
			FCommentBlock* pCommentBlock = CreateCommentBlockFromJson(commentBlockJson);
			page.CommentBlocks[pageAddr] = pCommentBlock;
		}
	}

	if (jsonDoc.contains("LabelInfo"))
	{
		for (const auto labelInfoJson : jsonDoc["LabelInfo"])
		{
			const uint16_t pageAddr = labelInfoJson["Address"];
			FLabelInfo* pLabelInfo = CreateLabelInfoFromJson(labelInfoJson);
			page.Labels[pageAddr] = pLabelInfo;
		}
	}

	if (jsonDoc.contains("CodeInfo"))
	{
		for (const auto codeInfoJson : jsonDoc["CodeInfo"])
		{
			const uint16_t pageAddr = codeInfoJson["Address"];
			FCodeInfo* pCodeInfo = CreateCodeInfoFromJson(codeInfoJson);
			page.CodeInfo[pageAddr] = pCodeInfo;
		}
	}

	if (jsonDoc.contains("DataInfo"))
	{
		for (const auto dataInfoJson : jsonDoc["DataInfo"])
		{
			const uint16_t pageAddr = dataInfoJson["Address"];
			FDataInfo* pDataInfo = &page.DataInfo[pageAddr];
			LoadDataInfoFromJson(pDataInfo, dataInfoJson);
		}
	}
}