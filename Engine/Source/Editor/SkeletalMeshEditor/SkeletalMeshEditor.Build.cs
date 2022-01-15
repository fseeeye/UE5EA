// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SkeletalMeshEditor : ModuleRules
{
	public SkeletalMeshEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Persona",
            }
        );

        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"EditorFramework",
                "UnrealEd",
                "SkeletonEditor",
                "Kismet",
                "KismetWidgets",
                "ActorPickerMode",
                "SceneDepthPickerMode",
                "MainFrame",
                "DesktopPlatform",
                "PropertyEditor",
                "RHI",
                "ClothingSystemRuntimeCommon",
                "ClothingSystemEditorInterface",
				"ClothingSystemRuntimeInterface",
				"SkeletalMeshUtilitiesCommon",
				"ToolMenus",
				"EditorSubsystem",
				"StatusBar",
			}
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "PropertyEditor",
            }
        );

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
            }
		);
	}
}