#pragma once

class SettingsIni
{
public:
	// General
	static inline int  iVerboseMode = 1;
	static inline bool bRagdollStabilization = true;

	static bool ReadSettings()
	{
		constexpr auto path = L"Data/SKSE/Plugins/DeathIdleFix.ini";

		if (!std::filesystem::exists(path)) return false;

		CSimpleIniA ini;
		ini.SetUnicode();
		SI_Error rc = ini.LoadFile(path);

		if (rc < 0) return false;

		// General
		iVerboseMode = ini.GetLongValue("General", "iVerboseMode", 1);
		bRagdollStabilization = ini.GetBoolValue("General", "bRagdollStabilization", true);

		debugVerboseMode = iVerboseMode;

		return true;
	}
};
