// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualCameraEditorStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"

// This is to fix the issue that SlateStyleMacros like IMAGE_BRUSH look for RootToContentDir but StyleSet->RootToContentDir is how this style is set up
#define RootToContentDir VirtualCameraStyle::StyleInstance->RootToContentDir

namespace VirtualCameraStyle
{
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);

	const FName NAME_StyleName(TEXT("VirtualCameraStyle"));

	static TUniquePtr<FSlateStyleSet> StyleInstance;
}

void FVirtualCameraEditorStyle::Register()
{
	VirtualCameraStyle::StyleInstance = MakeUnique<FSlateStyleSet>(VirtualCameraStyle::NAME_StyleName);
	const FString ProjectBaseDir = IPluginManager::Get().FindPlugin(TEXT("VirtualCamera"))->GetBaseDir();
	const FString ContentRoot = ProjectBaseDir / TEXT("Content/Editor/Icons/");
	VirtualCameraStyle::StyleInstance->SetContentRoot(ContentRoot);

	VirtualCameraStyle::StyleInstance->Set("TabIcons.VirtualCamera.Small", new IMAGE_BRUSH_SVG("VirtualCamera", VirtualCameraStyle::Icon16x16));
	VirtualCameraStyle::StyleInstance->Set("VirtualCamera.Stream", new IMAGE_BRUSH("VirtualCamera_Stream_40x", VirtualCameraStyle::Icon40x40));
	VirtualCameraStyle::StyleInstance->Set("VirtualCamera.Stop", new IMAGE_BRUSH("VirtualCamera_Stop_40x", VirtualCameraStyle::Icon40x40));

	FSlateStyleRegistry::RegisterSlateStyle(*VirtualCameraStyle::StyleInstance.Get());
}

void FVirtualCameraEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*VirtualCameraStyle::StyleInstance.Get());
	VirtualCameraStyle::StyleInstance.Reset();
}

FName FVirtualCameraEditorStyle::GetStyleSetName()
{
	return VirtualCameraStyle::NAME_StyleName;
}

const ISlateStyle& FVirtualCameraEditorStyle::Get()
{
	check(VirtualCameraStyle::StyleInstance.IsValid());
	return *VirtualCameraStyle::StyleInstance.Get();
}

#undef IMAGE_BRUSH
