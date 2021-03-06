// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using EpicGames.Core;

public class MLSDK : ModuleRules
{
	public MLSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		// Needed for FVector, FQuat and FTransform used in MagicLeapMath.h
		PublicDependencyModuleNames.Add("Core");
		// Include headers to be public to other modules.
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

		UEBuildPlatformSDK SDK = UEBuildPlatformSDK.GetSDKForPlatform(UnrealTargetPlatform.Lumin.ToString());
		string MLSDKPath = System.Environment.GetEnvironmentVariable("MLSDK");
		bool bIsMLSDKInstalled = false;
		bool bIsSupportedTargetPlatformAndType = ((Target.Platform == UnrealTargetPlatform.Lumin) || (Target.Type == TargetRules.TargetType.Editor));
		if (SDK != null && SDK.IsVersionValid(SDK.GetInstalledVersion(), bForAutoSDK:false) && !string.IsNullOrEmpty(MLSDKPath))
		{
			string IncludePath = Path.Combine(MLSDKPath, "include");
			string LibraryPath = Path.Combine(MLSDKPath, "lib");
			string LibraryPlatformFolder = string.Empty;
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				LibraryPlatformFolder = "win64";
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				LibraryPlatformFolder = "osx";
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				LibraryPlatformFolder = "linux64";
			}
			else if (Target.Platform == UnrealTargetPlatform.Lumin)
			{
				LibraryPlatformFolder = "lumin";
			}
			else
			{
				// This will fail the bIsMLSDKInstalled check, causing WITH_MLSDK to be set to 0 for unsupported platforms.
				LibraryPlatformFolder = "unsupported";
			}

			LibraryPath = Path.Combine(LibraryPath, LibraryPlatformFolder);

			bIsMLSDKInstalled = Directory.Exists(IncludePath) && Directory.Exists(LibraryPath);
			if (bIsMLSDKInstalled)
			{
				PublicIncludePaths.Add(IncludePath);
				if (Target.Platform == UnrealTargetPlatform.Lumin)
				{
					PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "Vulkan/Include/vulkan");
				}
			}
		}

		PublicDefinitions.Add("WITH_MLSDK=" + (bIsMLSDKInstalled && bIsSupportedTargetPlatformAndType ? "1" : "0"));
	}
}
