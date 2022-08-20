#include "CodeAnaysisPage.h"
#include "CodeAnalyser.h"

#include "Util/MemoryBuffer.h"
#include <cassert>

//#include "json.hpp"

void FCodeAnalysisPage::Initialise(uint16_t address)
{
	BaseAddress = address;
	
	memset(Labels, 0, sizeof(Labels));
	memset(CodeInfo, 0, sizeof(CodeInfo));
	//memset(DataInfo, 0, sizeof(DataInfo));
	memset(LastWriter, 0, sizeof(LastWriter));

	for (int addr = 0; addr < FCodeAnalysisPage::kPageSize; addr++)
	{
		// set up data entry for address
		FDataInfo& dataInfo = DataInfo[addr];
		dataInfo.Address = BaseAddress + (uint16_t)addr;
		dataInfo.ByteSize = 1;
		dataInfo.DataType = DataType::Byte;
	}
}

void FCodeAnalysisPage::Reset(void)
{
	for (int addr = 0; addr < FCodeAnalysisPage::kPageSize; addr++)
	{
		if (Labels[addr] != nullptr)
		{
			delete Labels[addr];
			Labels[addr] = nullptr;
		}

		if (CodeInfo[addr] != nullptr)
		{
			FCodeInfo* pCodeInfo = CodeInfo[addr];
			for(int i=0;i<pCodeInfo->ByteSize;i++)
				CodeInfo[addr + i] = nullptr;
			delete pCodeInfo;
		}
		
		/*if (DataInfo[addr] != nullptr)
		{
			delete DataInfo[addr];
			DataInfo[addr] = nullptr;
		}*/
	}

	Initialise(BaseAddress);
}

static const uint32_t kMagic = 0xc0de;
static const uint32_t kVersionNo = 2;

uint32_t kLabelMagic = 'LABL';
uint32_t kCodeMagic = 'CODE';
uint32_t kDataMagic = 'DATA';

void WriteItemToBuffer(const FItem& item, FMemoryBuffer& buffer)
{
	buffer.WriteString(item.Comment);
	//buffer.Write(item.Address);
	buffer.Write(item.ByteSize);
}

void ReadItemFromBuffer( FItem& item, FMemoryBuffer& buffer)
{
	item.Comment = buffer.ReadString();
	//buffer.Read(item.Address);
	buffer.Read(item.ByteSize);
}

void WriteReferencesToBuffer(const std::map<uint16_t, int>& references, FMemoryBuffer& buffer)
{
	buffer.Write((uint16_t)references.size());
	for (const auto& ref : references)
	{
		buffer.Write(ref.first);
	}
}

void ReadReferencesFromBuffer(std::map<uint16_t, int>& references, FMemoryBuffer& buffer)
{
	const uint16_t count = buffer.Read<uint16_t>();
	for (int i = 0; i < count; i++)
	{
		const uint16_t addr = buffer.Read<uint16_t>();
		references[addr] = 1;
	}
}

void FCodeAnalysisPage::WriteToBuffer(FMemoryBuffer& buffer)
{
	buffer.Write(kMagic);
	buffer.Write(kVersionNo);

	buffer.Write(BaseAddress);

	buffer.Write(kLabelMagic);
	for (int i = 0; i < kPageSize; i++)
	{
		if (Labels[i] != nullptr)
		{
			const FLabelInfo& label = *Labels[i];
			buffer.Write<uint16_t>(i);	// address in page
			WriteItemToBuffer(label, buffer);
			buffer.Write((uint8_t)label.LabelType);
			buffer.WriteString(label.Name);
			buffer.Write(label.Global);
			// References?
			WriteReferencesToBuffer(label.References, buffer);
		}
	}
	buffer.Write<uint16_t>(0xffff);	// terminator

	// Code Section
	buffer.Write(kCodeMagic);
	for (int i = 0; i < kPageSize; i++)
	{
		if (CodeInfo[i] != nullptr)
		{
			const FCodeInfo& codeInfo = *CodeInfo[i];
			if (codeInfo.Address == BaseAddress + i)	// only first item
			{
				buffer.Write<uint16_t>(i);	// address in page
				WriteItemToBuffer(codeInfo, buffer);

				buffer.Write<uint16_t>(codeInfo.JumpAddress);
				buffer.Write<uint16_t>(codeInfo.PointerAddress);
				buffer.Write<uint32_t>(codeInfo.Flags);
			}
		}
	}
	buffer.Write<uint16_t>(0xffff);	// terminator

	// Data Section
	buffer.Write(kDataMagic);
	for (int i = 0; i < kPageSize; i++)
	{
		//if (DataInfo[i] != nullptr)
		{
			const FDataInfo& dataInfo = DataInfo[i];
			buffer.Write<uint16_t>(i);	// address in page
			WriteItemToBuffer(dataInfo, buffer);

			buffer.Write<uint8_t>((uint8_t)dataInfo.DataType);
			WriteReferencesToBuffer(dataInfo.Reads, buffer);
			WriteReferencesToBuffer(dataInfo.Writes, buffer);
		}
	}
	buffer.Write<uint16_t>(0xffff);	// terminator

}

bool FCodeAnalysisPage::ReadFromBuffer(FMemoryBuffer& buffer)
{
	if (buffer.Read<uint32_t>() != kMagic)
		return false;

	const uint32_t fileVersion = buffer.Read<uint32_t>();
	if (fileVersion != kVersionNo)
		return false;

	buffer.Read(BaseAddress);

	// Read Labels
	assert(buffer.Read<uint32_t>() == kLabelMagic);

	while (true)
	{
		const uint16_t pageAddr = buffer.Read<uint16_t>();
		if (pageAddr == 0xffff)
			break;

		FLabelInfo* pNewLabel = new FLabelInfo;
		ReadItemFromBuffer(*pNewLabel, buffer);
		pNewLabel->Address = BaseAddress + pageAddr;
		pNewLabel->LabelType = (LabelType)buffer.Read<uint8_t>();
		pNewLabel->Name = buffer.ReadString();
		buffer.Read(pNewLabel->Global);
		ReadReferencesFromBuffer(pNewLabel->References, buffer);

		Labels[pageAddr] = pNewLabel;
	}

	// Read Code
	assert(buffer.Read<uint32_t>() == kCodeMagic);

	while (true)
	{
		const uint16_t pageAddr = buffer.Read<uint16_t>();
		if (pageAddr == 0xffff)
			break;

		FCodeInfo* pNewCodeInfo = new FCodeInfo;
		ReadItemFromBuffer(*pNewCodeInfo, buffer);
		pNewCodeInfo->Address = BaseAddress + pageAddr;
		buffer.Read(pNewCodeInfo->JumpAddress);
		buffer.Read(pNewCodeInfo->PointerAddress);
		buffer.Read(pNewCodeInfo->Flags);

		for(int i=0;i<pNewCodeInfo->ByteSize;i++)
			CodeInfo[pageAddr + i] = pNewCodeInfo;
	}

	// Read Data
	assert(buffer.Read<uint32_t>() == kDataMagic);

	while (true)
	{
		const uint16_t pageAddr = buffer.Read<uint16_t>();
		if (pageAddr == 0xffff)
			break;

		FDataInfo& dataInfo = DataInfo[pageAddr];
		ReadItemFromBuffer(dataInfo, buffer);
		dataInfo.Address = BaseAddress + pageAddr;
		dataInfo.DataType = (DataType)buffer.Read<uint8_t>();
		ReadReferencesFromBuffer(dataInfo.Reads, buffer);
		ReadReferencesFromBuffer(dataInfo.Writes, buffer);
	}

	return true;
}

void FCodeAnalysisPage::SetLabelAtAddress(const char* pLabelName, LabelType type, uint16_t addr)
{
	FLabelInfo* pLabel = Labels[addr];
	if (pLabel == nullptr)
	{
		pLabel = new FLabelInfo;
		Labels[addr] = pLabel;
	}

	pLabel->Name = pLabelName;
	pLabel->LabelType = type;
}


#if 0
void FCodeAnalysisPage::WriteToJSon(nlohmann::json& jsonOutput)
{
	bool bInCode = CodeInfo[0] != nullptr;
	int sectionSize = 0;

	// determine sections
	for (int addr = 0; addr < FCodeAnalysisPage::kPageSize; addr++)
	{
		nlohmann::json addressInfo;

		const FLabelInfo* pLabelInfo = Labels[addr];
		if (pLabelInfo)
		{
			nlohmann::json labelInfo;
			// TODO: write Label Info
			addressInfo["Label"] = labelInfo;
		}

		jsonOutput.push_back(addressInfo);
	}
}
#endif