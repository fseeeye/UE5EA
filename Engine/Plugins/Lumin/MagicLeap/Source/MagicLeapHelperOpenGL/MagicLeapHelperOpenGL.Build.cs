// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MagicLeapHelperOpenGL : ModuleRules
	{
		public MagicLeapHelperOpenGL(ReadOnlyTargetRules Target) : base(Target)
		{
			// Include headers to be public to other modules.
			PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"Engine",
					"RenderCore",
					"RHI",
					"RHICore",
					"MLSDK"
				});

			// TODO: Explore linking Unreal modules against a commong header and
			// having a runtime dll linking against the library according to the platform.
			if (Target.Platform != UnrealTargetPlatform.Mac && Target.Platform != UnrealTargetPlatform.IOS)
			{
				PrivateDependencyModuleNames.Add("OpenGLDrv");
				string EngineSourceDirectory = "../../../../Source";

				if (Target.Platform == UnrealTargetPlatform.Linux)
				{
					PrivateIncludePaths.AddRange(
						new string[] {
							Path.Combine(EngineSourceDirectory, "ThirdParty/SDL2/SDL-gui-backend/include"),
						}
					);
				}

				PrivateIncludePaths.AddRange(
					new string[] {
						"MagicLeapHelperOpenGL/Private",
						Path.Combine(EngineSourceDirectory, "Runtime/OpenGLDrv/Private")
					});

				AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
			}

		}
	}
}
