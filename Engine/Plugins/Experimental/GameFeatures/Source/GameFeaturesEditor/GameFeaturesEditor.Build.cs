// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameFeaturesEditor : ModuleRules
	{
        public GameFeaturesEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
                {
                    "Core",
                    "CoreUObject",
                }
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DeveloperSettings",
					"Engine",
					"ModularGameplay",
					"GameFeatures",
					"EditorSubsystem",
					"UnrealEd",
					"Projects",
					"EditorFramework",
					"Slate",
					"SlateCore",
					"EditorStyle",
					"PropertyEditor",
					"SharedSettingsWidgets",
					"Json"
				}
			);
		}
	}
}
