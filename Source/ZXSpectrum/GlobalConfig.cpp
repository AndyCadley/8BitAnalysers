#include "GlobalConfig.h"

#include "json.hpp"

#include <iomanip>
#include <fstream>
#include <sstream>

using json = nlohmann::json;


FGlobalConfig	g_GlobalConfig;

FGlobalConfig& GetGlobalConfig()
{
	return g_GlobalConfig;
}

bool LoadGlobalConfig(const char* fileName)
{
	FGlobalConfig& config = GetGlobalConfig();

	std::ifstream inFileStream(fileName);
	if (inFileStream.is_open() == false)
		return false;

	json jsonConfigFile;

	inFileStream >> jsonConfigFile;
	inFileStream.close();

	config.bEnableAudio = jsonConfigFile["EnableAudio"];
	config.bShowScanLineIndicator = jsonConfigFile["ShowScanlineIndicator"];
	if(jsonConfigFile.contains("ShowOpcodeValues"))
		config.bShowOpcodeValues = jsonConfigFile["ShowOpcodeValues"];
	config.LastGame = jsonConfigFile["LastGame"];
	config.NumberDisplayMode = (ENumberDisplayMode)jsonConfigFile["NumberMode"];
	return true;
}

bool SaveGlobalConfig(const char* fileName)
{
	const FGlobalConfig& config = GetGlobalConfig();
	json jsonConfigFile;

	jsonConfigFile["EnableAudio"] = config.bEnableAudio;
	jsonConfigFile["ShowScanlineIndicator"] = config.bShowScanLineIndicator;
	jsonConfigFile["ShowOpcodeValues"] = config.bShowOpcodeValues;
	jsonConfigFile["LastGame"] = config.LastGame;
	jsonConfigFile["NumberMode"] = (int)config.NumberDisplayMode;

	std::ofstream outFileStream(fileName);
	if (outFileStream.is_open())
	{
		outFileStream << std::setw(4) << jsonConfigFile << std::endl;
		return true;
	}

	return false;
}