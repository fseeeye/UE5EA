// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System;
public class GeForceNOW : ModuleRules
{
    public GeForceNOW(ReadOnlyTargetRules Target)
        : base(Target)
	{
		Type = ModuleType.External;


        if (Target.Type != TargetRules.TargetType.Server
			&& Target.Configuration != UnrealTargetConfiguration.Unknown
			&& Target.Configuration != UnrealTargetConfiguration.Debug
            && Target.Platform == UnrealTargetPlatform.Win64)
		{
            String GFNPath = Target.UEThirdPartySourceDirectory + "NVIDIA/GeForceNOW/";
            PublicSystemIncludePaths.Add(GFNPath + "include");
            
            String GFNLibPath = GFNPath + "lib/x64/";
            PublicAdditionalLibraries.Add(GFNLibPath + "GfnRuntimeMD.lib");

            PublicDefinitions.Add("NV_GEFORCENOW=1");
        }
		else
        {
            PublicDefinitions.Add("NV_GEFORCENOW=0");
        }
	}
}

