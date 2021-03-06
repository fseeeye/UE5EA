// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EditorInteractiveToolsFramework : ModuleRules
{
	public EditorInteractiveToolsFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
                //"ContentBrowser"
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"TypedElementFramework",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"RenderCore",
				"Slate",
                "SlateCore",
                "InputCore",
				"EditorFramework",
				"UnrealEd",
                "ContentBrowser",
                "LevelEditor",
                "ApplicationCore",
                "EditorStyle",
                "InteractiveToolsFramework",
				"MeshDescription",
				"StaticMeshDescription"

				// ... add private dependencies that you statically link with here ...	
			}
            );
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
