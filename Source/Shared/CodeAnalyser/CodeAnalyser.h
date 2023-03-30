#pragma once
#include <cstdint>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <algorithm>

#include "CodeAnalyserTypes.h"
#include "CodeAnaysisPage.h"

class FGraphicsView;
class FCodeAnalysisState;

enum class ELabelType;

// CPU abstraction
enum class ECPUType
{
	Unknown,
	Z80,
	M6502
};

/* the input callback type */
typedef uint8_t(*FDasmInput)(void* user_data);
/* the output callback type */
typedef void (*FDasmOutput)(char c, void* user_data);

class ICPUInterface
{
public:
	// Memory Access
	virtual uint8_t		ReadByte(uint16_t address) const = 0;
	virtual uint16_t	ReadWord(uint16_t address) const = 0;
	virtual const uint8_t*	GetMemPtr(uint16_t address) const = 0;
	virtual void		WriteByte(uint16_t address, uint8_t value) = 0;

	virtual uint16_t	GetPC(void) = 0;
	virtual uint16_t	GetSP(void) = 0;

	// breakpoints
	virtual bool	IsAddressBreakpointed(uint16_t addr) = 0;
	virtual bool	ToggleExecBreakpointAtAddress(uint16_t addr) = 0;
	virtual bool	ToggleDataBreakpointAtAddress(uint16_t addr, uint16_t dataSize) = 0;

	// commands
	virtual void	Break() = 0;
	virtual void	Continue() = 0;
	virtual void	StepOver() = 0;
	virtual void	StepInto() = 0;
	virtual void	StepFrame() = 0;
	virtual void	StepScreenWrite() = 0;
	virtual void	GraphicsViewerSetView(uint16_t address, int charWidth) = 0;

	virtual bool	ShouldExecThisFrame(void) const = 0;
	virtual bool	IsStopped(void) const = 0;

	virtual void* GetCPUEmulator(void) { return nullptr; }	// get pointer to emulator - a bit of a hack

	ECPUType	CPUType = ECPUType::Unknown;
};

typedef void (*z80dasm_output_t)(char c, void* user_data);

class IDasmNumberOutput
{
public:

	virtual void OutputU8(uint8_t val, z80dasm_output_t out_cb) = 0;
	virtual void OutputU16(uint16_t val, z80dasm_output_t out_cb) = 0;
	virtual void OutputD8(int8_t val, z80dasm_output_t out_cb) = 0;
};

class FDasmStateBase : public IDasmNumberOutput
{
public:
	FCodeAnalysisState*		CodeAnalysisState;
	uint16_t				CurrentAddress;
	std::string				Text;
};

struct FMemoryAccess
{
	uint16_t	Address;
	uint8_t		Value;
	uint16_t	PC;
};


enum class EKey
{
	SetItemData,
	SetItemText,
	SetItemCode,
	SetItemImage,
	ToggleItemBinary,

	AddLabel,
	Rename,
	Comment,
	AddCommentBlock,

	// Debugger
	BreakContinue,
	StepOver,
	StepInto,
	StepFrame,
	StepScreenWrite,
	Breakpoint,

	Count
};


struct FDataFormattingOptions
{
	EDataType	DataType = EDataType::Byte;
	int			StartAddress = 0;
	int			ItemSize = 1;
	int			NoItems = 1;
	uint16_t	CharacterSet = 0;
	uint8_t		EmptyCharNo = 0;
	bool		ClearCodeInfo = false;
	bool		ClearLabels = false;
	bool		AddLabelAtStart = false;

	bool		IsValid() const {	return NoItems > 0 && ItemSize > 0;	}
	uint16_t	CalcEndAddress() const { return StartAddress + (NoItems * ItemSize) - 1; }
	void		SetupForBitmap(uint16_t address, int xSize, int ySize)
	{
		DataType = EDataType::Bitmap;
		StartAddress = address;
		ItemSize = xSize / 8;
		NoItems = ySize;
	}

	void		SetupForCharmap(uint16_t address, int xSize, int ySize)
	{
		DataType = EDataType::CharacterMap;
		StartAddress = address;
		ItemSize = xSize;
		NoItems = ySize;
	}
};

struct FLabelListFilter
{
	std::string		FilterText;
	uint16_t		MinAddress = 0x4000;
	uint16_t		MaxAddress = 0xffff;
};

struct FCodeAnalysisItem
{
	FCodeAnalysisItem() : BankId(-1), Address(0) {}
	//FCodeAnalysisItem(FItem* pItem, uint16_t addr) :Item(pItem), BankId(-1), Address(addr) {}	// temp until we get refs working properly
	FCodeAnalysisItem(FItem* pItem, int16_t bankId, uint16_t addr) :Item(pItem), BankId(bankId), Address(addr) {}
	FCodeAnalysisItem(FItem* pItem, FAddressRef addr) :Item(pItem), AddressRef(addr) {}

	bool IsValid() const { return Item != nullptr; }
	
	FItem* Item = nullptr;
	union
	{
		struct
		{
			int16_t		BankId;
			uint16_t	Address;	// address in address space
		};
		FAddressRef		AddressRef;
	};
};

// view state for code analysis window
struct FCodeAnalysisViewState
{
	// accessor functions
	const FCodeAnalysisItem& GetCursorItem() const { return CursorItem; }
	void SetCursorItem(const FCodeAnalysisItem& item)
	{
		CursorItem = item;
	}
		
	FAddressRef& GetGotoAddress() { return GoToAddressRef; }
	const FAddressRef& GetGotoAddress() const { return GoToAddressRef; }
	void GoToAddress(FAddressRef address, bool bLabel = false);
	bool GoToPreviousAddress();

	bool			Enabled = false;
	bool			TrackPCFrame = false;
	FAddressRef		HoverAddress;		// address being hovered over
	FAddressRef		HighlightAddress;	// address to highlight
	bool			GoToLabel = false;
	int16_t			ViewingBankId = -1;
	// for global Filters
	bool						ShowROMLabels = false;
	FLabelListFilter			GlobalDataItemsFilter;
	std::vector<FCodeAnalysisItem>	FilteredGlobalDataItems;
	FLabelListFilter				GlobalFunctionsFilter;
	std::vector<FCodeAnalysisItem>	FilteredGlobalFunctions;


	bool					DataFormattingTabOpen = false;
	FDataFormattingOptions	DataFormattingOptions;
private:
	FCodeAnalysisItem			CursorItem;
	FAddressRef					GoToAddressRef;
	std::vector<FAddressRef>	AddressStack;
};

struct FCodeAnalysisConfig
{
	bool				bShowOpcodeValues = false;
	const uint32_t*		CharacterColourLUT = nullptr;
};

// Analysis memory bank
struct FCodeAnalysisBank
{
	int16_t				Id = -1;
	int					NoPages = 0;
	std::vector<int>	MappedPages;	// banks can be mapped to multiple pages
	int					PrimaryMappedPage = -1;
	uint8_t*			Memory = nullptr;	// pointer to memory bank occupies
	FCodeAnalysisPage*	Pages;
	std::string			Name;
	bool				bReadOnly = false;
	bool				bIsDirty = false;
	std::vector<FCodeAnalysisItem>	ItemList;
};



struct FWatch : public FAddressRef
{
	FWatch() = default;
	FWatch(const FAddressRef addressRef) : FAddressRef(addressRef) {}
	FWatch(int16_t bankId, uint16_t address) : FAddressRef(bankId, address) {}
};


// code analysis information
class FCodeAnalysisState
{
public:
	// constants
	static const int kAddressSize = 1 << 16;
	static const int kPageShift = 10;
	static const int kPageMask = 1023;
	static const int kNoPagesInAddressSpace = kAddressSize / FCodeAnalysisPage::kPageSize;

	void	Init(ICPUInterface* pCPUInterface);

	ICPUInterface* CPUInterface = nullptr;
	int						CurrentFrameNo = 0;

	// Memory Banks & Pages
	int16_t		CreateBank(const char* name, int noKb, uint8_t* pMemory, bool bReadOnly);
	bool		MapBank(int16_t bankId, int startPageNo);
	bool		UnMapBank(int16_t bankId, int startPageNo);
	bool		IsBankIdMapped(int16_t bankId) const;

	bool		MapBankForAnalysis(FCodeAnalysisBank& bank);
	void		UnMapAnalysisBanks();
	
	FCodeAnalysisBank* GetBank(int16_t bankId);
	int16_t		GetBankFromAddress(uint16_t address) const { return MappedBanks[address >> kPageShift]; }
	const std::vector<FCodeAnalysisBank>& GetBanks() const { return Banks; }
	std::vector<FCodeAnalysisBank>& GetBanks() { return Banks; }

	uint8_t		ReadByte(uint16_t address) const
	{
		if (MappedMem[address >> kPageShift] == nullptr)
			return CPUInterface->ReadByte(address);
		else
			return *(MappedMem[(address >> kPageShift)] + (address & kPageMask));
	}

	uint16_t	ReadWord(uint16_t address) const
	{
		if (MappedMem[address >> kPageShift] == nullptr)
			return CPUInterface->ReadWord(address);
		else
			return *(uint16_t*)(MappedMem[address >> kPageShift] + (address & kPageMask));
	}

	void		WriteByte(uint16_t address, uint8_t value) 
	{ 
		if (MappedMem[address >> kPageShift] == nullptr)
			CPUInterface->WriteByte(address, value);
		else
			*(MappedMem[(address >> kPageShift)] + (address & kPageMask)) = value;
	}
	
	FCodeAnalysisPage* GetPage(int16_t id) { return RegisteredPages[id]; }

	void	SetCodeAnalysisDirty(FAddressRef addrRef)
	{
		FCodeAnalysisBank* pBank = GetBank(addrRef.BankId);
		if (pBank != nullptr)
			pBank->bIsDirty = true;
		bCodeAnalysisDataDirty = true;
	}

	void	SetCodeAnalysisDirty(uint16_t address)	
	{ 
		SetCodeAnalysisDirty({ GetBankFromAddress(address),address });
	}

	void	SetAddressRangeDirty()
	{
		for (int i = 0; i < kNoPagesInAddressSpace; i++)
		{
			FCodeAnalysisBank* pBank = GetBank(MappedBanks[i]);
			if (pBank != nullptr)
				pBank->bIsDirty = true;
			bCodeAnalysisDataDirty = true;
		}
	}

	void	SetAllBanksDirty()
	{
		for (auto& bank : Banks)
			bank.bIsDirty = true;
		bCodeAnalysisDataDirty = true;
	}

	void	ClearDirtyStatus(void)
	{
		bCodeAnalysisDataDirty = false;
	}
	
	bool IsCodeAnalysisDataDirty() const { return bCodeAnalysisDataDirty; }
	void ClearRemappings() { bMemoryRemapped = false; }
	bool HasMemoryBeenRemapped() const { return bMemoryRemapped; }
	//const std::vector<int16_t>& GetDirtyBanks() const { return RemappedBanks; }

	void	ResetLabelNames() { LabelUsage.clear(); }
	bool	EnsureUniqueLabelName(std::string& lableName);
	bool	RemoveLabelName(const std::string& labelName);	// for changing label names

	// Watches
	void InitWatches() { Watches.clear(); }
	void	AddWatch(FWatch watch)
	{
		Watches.push_back(watch);
	}

	bool	RemoveWatch(FWatch watch)
	{
		for (auto watchIt = Watches.begin(); watchIt != Watches.end(); ++watchIt)
		{
			if(*watchIt == watch)
				Watches.erase(watchIt);
		}

		return true;
	}

	const std::vector<FWatch>& GetWatches() const { return Watches; }

public:

	bool					bRegisterDataAccesses = true;

	std::vector<FCodeAnalysisItem>	ItemList;

	std::vector<FCodeAnalysisItem>	GlobalDataItems;
	bool						bRebuildFilteredGlobalDataItems = true;
	
	std::vector<FCodeAnalysisItem>	GlobalFunctions;
	bool						bRebuildFilteredGlobalFunctions = true;

	static const int kNoViewStates = 4;
	FCodeAnalysisViewState	ViewState[kNoViewStates];	// new multiple view states
	int						FocussedWindowId = 0;
	FCodeAnalysisViewState& GetFocussedViewState() { return ViewState[FocussedWindowId]; }
	FCodeAnalysisViewState& GetAltViewState() { return ViewState[FocussedWindowId ^ 1]; }
	
	std::vector<FWatch>		Watches;	
	std::vector<FCPUFunctionCall>	CallStack;
	uint16_t				StackMin;
	uint16_t				StackMax;

	std::vector<uint16_t>	FrameTrace;

	int						KeyConfig[(int)EKey::Count];

	std::vector< class FCommand *>	CommandStack;

	bool					bAllowEditing = false;

	FCodeAnalysisConfig Config;
public:
	// Access functions for code analysis

	FCodeAnalysisPage* GetReadPage(uint16_t addr)  
	{
		const int pageNo = addr >> kPageShift;
		FCodeAnalysisPage* pPage = ReadPageTable[pageNo];
		if (pPage == nullptr)
		{
			printf("Read page %d NOT mapped", pageNo);
		}

		return pPage;
	}
	
	const FCodeAnalysisPage* GetReadPage(uint16_t addr) const { return ((FCodeAnalysisState *)this)->GetReadPage(addr); }
	FCodeAnalysisPage* GetWritePage(uint16_t addr) 
	{
		const int pageNo = addr >> kPageShift;
		FCodeAnalysisPage* pPage = WritePageTable[pageNo];
		if (pPage == nullptr)
		{
			printf("Write page %d NOT mapped", pageNo);
		}

		return pPage;
	}
	const FCodeAnalysisPage* GetWritePage(uint16_t addr) const { return ((FCodeAnalysisState*)this)->GetWritePage(addr); }

	const FLabelInfo* GetLabelForAddress(uint16_t addr) const { return GetReadPage(addr)->Labels[addr & kPageMask]; }
	FLabelInfo* GetLabelForAddress(uint16_t addr) { return GetReadPage(addr)->Labels[addr & kPageMask]; }
	void SetLabelForAddress(uint16_t addr, FLabelInfo* pLabel) 
	{
		if(pLabel != nullptr)	// ensure no name clashes
			EnsureUniqueLabelName(pLabel->Name);
		GetReadPage(addr)->Labels[addr & kPageMask] = pLabel; 
	}

	FCommentBlock* GetCommentBlockForAddress(uint16_t addr) const { return GetReadPage(addr)->CommentBlocks[addr & kPageMask]; }
	void SetCommentBlockForAddress(uint16_t addr, FCommentBlock* pCommentBlock)
	{
		GetReadPage(addr)->CommentBlocks[addr & kPageMask] = pCommentBlock;
	}

	const FCodeInfo* GetCodeInfoForAddress(uint16_t addr) const { return GetReadPage(addr)->CodeInfo[addr & kPageMask]; }
	FCodeInfo* GetCodeInfoForAddress(uint16_t addr) { return GetReadPage(addr)->CodeInfo[addr & kPageMask]; }
	void SetCodeInfoForAddress(uint16_t addr, FCodeInfo* pCodeInfo) { GetReadPage(addr)->CodeInfo[addr & kPageMask] = pCodeInfo; }

	const FDataInfo* GetReadDataInfoForAddress(uint16_t addr) const { return &GetReadPage(addr)->DataInfo[addr & kPageMask]; }
	FDataInfo* GetReadDataInfoForAddress(uint16_t addr) { return &GetReadPage(addr)->DataInfo[addr & kPageMask]; }

	const FDataInfo* GetWriteDataInfoForAddress(uint16_t addr) const { return  &GetWritePage(addr)->DataInfo[addr & kPageMask]; }
	FDataInfo* GetWriteDataInfoForAddress(uint16_t addr) { return &GetWritePage(addr)->DataInfo[addr & kPageMask]; }

	uint16_t GetLastWriterForAddress(uint16_t addr) const { return GetWritePage(addr)->DataInfo[addr & kPageMask].LastWriter; }
	void SetLastWriterForAddress(uint16_t addr, uint16_t lastWriter) { GetWritePage(addr)->DataInfo[addr & kPageMask].LastWriter = lastWriter; }

	FMachineState* GetMachineState(uint16_t addr) { return GetReadPage(addr)->MachineState[addr & kPageMask];}
	void SetMachineStateForAddress(uint16_t addr, FMachineState* pMachineState) { GetReadPage(addr)->MachineState[addr & kPageMask] = pMachineState; }

	bool FindMemoryPattern(uint8_t* pData, size_t dataSize, uint16_t offset, uint16_t& outAddr);

	void FindAsciiStrings(uint16_t startAddress);


private:
	// private methods
	bool					RegisterPage(FCodeAnalysisPage* pPage, const char* pName)
	{
		if (pPage->PageId != -1)
			return false;
		pPage->PageId = (int16_t)RegisteredPages.size();
		RegisteredPages.push_back(pPage);
		PageNames.push_back(pName);
		return true;
	}
	const char* GetPageName(int16_t id) { return PageNames[id].c_str(); }
	int16_t					GetAddressReadPageId(uint16_t addr) { return GetReadPage(addr)->PageId; }
	int16_t					GetAddressWritePageId(uint16_t addr) { return GetWritePage(addr)->PageId; }
	const std::vector< FCodeAnalysisPage*>& GetRegisteredPages() const { return RegisteredPages; }


	void					SetCodeAnalysisReadPage(int pageNo, FCodeAnalysisPage* pPage) { ReadPageTable[pageNo] = pPage; if (pPage != nullptr) pPage->bUsed = true; }
	void					SetCodeAnalysisWritePage(int pageNo, FCodeAnalysisPage* pPage) { WritePageTable[pageNo] = pPage; if(pPage != nullptr) pPage->bUsed = true; }
	void					SetCodeAnalysisRWPage(int pageNo, FCodeAnalysisPage* pReadPage, FCodeAnalysisPage* pWritePage)
	{
		SetCodeAnalysisReadPage(pageNo, pReadPage);
		SetCodeAnalysisWritePage(pageNo, pWritePage);
	}

	// private data members

	FCodeAnalysisPage*				ReadPageTable[kNoPagesInAddressSpace];
	FCodeAnalysisPage*				WritePageTable[kNoPagesInAddressSpace];

	std::vector<FCodeAnalysisBank>	Banks;
	int16_t							MappedBanks[kNoPagesInAddressSpace];	// banks mapped into address space

	uint8_t*						MappedMem[kNoPagesInAddressSpace];	// mapped analysis memory
				
	std::vector<FCodeAnalysisPage*>	RegisteredPages;
	std::vector<std::string>	PageNames;
	int32_t						NextPageId = 0;
	std::map<std::string, int>	LabelUsage;

	bool						bCodeAnalysisDataDirty = false;
	bool						bMemoryRemapped = true;

};

// Analysis
FLabelInfo* GenerateLabelForAddress(FCodeAnalysisState &state, uint16_t pc, ELabelType label);
void RunStaticCodeAnalysis(FCodeAnalysisState &state, uint16_t pc);
bool RegisterCodeExecuted(FCodeAnalysisState &state, uint16_t pc, uint16_t nextpc);
void ReAnalyseCode(FCodeAnalysisState &state);
uint16_t WriteCodeInfoForAddress(FCodeAnalysisState& state, uint16_t pc);
void GenerateGlobalInfo(FCodeAnalysisState &state);
void RegisterDataRead(FCodeAnalysisState& state, uint16_t pc, uint16_t dataAddr);
void RegisterDataWrite(FCodeAnalysisState &state, uint16_t pc, uint16_t dataAddr, uint8_t value);
void UpdateCodeInfoForAddress(FCodeAnalysisState &state, uint16_t pc);
void ResetReferenceInfo(FCodeAnalysisState &state);

std::string GetItemText(FCodeAnalysisState& state, uint16_t address);

// Commands
void Undo(FCodeAnalysisState &state);

FLabelInfo* AddLabel(FCodeAnalysisState& state, uint16_t address, const char* name, ELabelType type);
FCommentBlock* AddCommentBlock(FCodeAnalysisState& state, uint16_t address);
FLabelInfo* AddLabelAtAddress(FCodeAnalysisState &state, uint16_t address);
void RemoveLabelAtAddress(FCodeAnalysisState &state, uint16_t address);
void SetLabelName(FCodeAnalysisState &state, FLabelInfo *pLabel, const char *pText);
void SetItemCode(FCodeAnalysisState& state, uint16_t addr);
//void SetItemCode(FCodeAnalysisState &state, const FCodeAnalysisItem& item);
void SetItemData(FCodeAnalysisState &state, const FCodeAnalysisItem& item);
void SetItemText(FCodeAnalysisState &state, const FCodeAnalysisItem& item);
void SetItemImage(FCodeAnalysisState& state, const FCodeAnalysisItem& item);
void SetItemCommentText(FCodeAnalysisState &state, const FCodeAnalysisItem& item, const char *pText);

void FormatData(FCodeAnalysisState& state, const FDataFormattingOptions& options);

// number output abstraction
IDasmNumberOutput* GetNumberOutput();
void SetNumberOutput(IDasmNumberOutput* pNumberOutputObj);

// machine state
FMachineState* AllocateMachineState(FCodeAnalysisState& state);
void FreeMachineStates(FCodeAnalysisState& state);
void CaptureMachineState(FMachineState* pMachineState, ICPUInterface* pCPUInterface);