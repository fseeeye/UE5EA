// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MagicLeapMediaEditor : ModuleRules
	{
		public MagicLeapMediaEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"EditorFramework",
					"MediaAssets",
					"UnrealEd",
                }
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"MagicLeapMediaEditor/Private",
				}
			);
		}
	}
}
