// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ImgMediaEditor : ModuleRules
	{
		public ImgMediaEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"DesktopWidgets",
					"EditorFramework",
					"EditorStyle",
					"ImgMedia",
					"MediaAssets",
					"Slate",
					"SlateCore",
					"UnrealEd",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"AssetTools",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"ImgMediaEditor/Private",
					"ImgMediaEditor/Private/Customizations",
					"ImgMediaEditor/Private/Factories",
				});
		}
	}
}
