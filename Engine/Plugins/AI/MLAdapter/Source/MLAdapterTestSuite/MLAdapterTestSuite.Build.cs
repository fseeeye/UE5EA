// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
    public class MLAdapterTestSuite : ModuleRules
    {
        public MLAdapterTestSuite(ReadOnlyTargetRules Target) : base(Target)
        {
            // rcplib is using exceptions so we have to enable that
            bEnableExceptions = true;
            PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

            PublicIncludePaths.AddRange(new string[] { });

            PublicDependencyModuleNames.AddRange(
                new string[] {
                        "Core",
                        "CoreUObject",
                        "Engine",
                        "AIModule",
                        "AITestSuite",
                        "MLAdapter",
                        "JsonUtilities",
                }
                );

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("EditorFramework");
                PrivateDependencyModuleNames.Add("UnrealEd");
            }

            // RPCLib disabled on other platforms at the moment
            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                PublicDefinitions.Add("WITH_RPCLIB=1");
                AddEngineThirdPartyPrivateStaticDependencies(Target, "RPCLib");

                string RPClibDir = Path.Combine(Target.UEThirdPartySourceDirectory, "rpclib");
                PublicIncludePaths.Add(Path.Combine(RPClibDir, "Source", "include"));
            }
            else
            {
                PublicDefinitions.Add("WITH_RPCLIB=0");
            }
        }
    }
}