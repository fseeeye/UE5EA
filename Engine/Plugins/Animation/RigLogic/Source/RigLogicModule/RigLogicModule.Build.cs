// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
    public class RigLogicModule : ModuleRules
    {
        private string ModulePath
        {
            get { return ModuleDirectory; }
        }

        public RigLogicModule(ReadOnlyTargetRules Target) : base(Target)
        {
            if (Target.LinkType != TargetLinkType.Monolithic)
            {
                PublicDefinitions.Add("RL_SHARED=1");
            }

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "ControlRig",
                    "RigLogicLib",
                    "RigVM",
                    "Projects",
					"RenderCore",
					"RHI"
                }
            );

            if (Target.Type == TargetType.Editor)
            {
                PublicDependencyModuleNames.Add("UnrealEd");
				PublicDependencyModuleNames.Add("MessageLog");
			}

			PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "AnimationCore"
                }
            );
        }
    }
}
