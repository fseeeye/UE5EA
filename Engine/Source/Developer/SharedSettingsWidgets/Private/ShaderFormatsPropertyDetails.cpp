// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderFormatsPropertyDetails.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Layout/Margin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorDirectories.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "IDetailPropertyRow.h"
#include "RHI.h"

#define LOCTEXT_NAMESPACE "ShaderFormatsPropertyDetails"

FText FShaderFormatsPropertyDetails::GetFriendlyNameFromRHINameMac(const FString& InRHIName)
{
	FText FriendlyRHIName = FText::FromString(InRHIName);
	
	FName RHIName(*InRHIName, FNAME_Find);
	EShaderPlatform Platform = ShaderFormatToLegacyShaderPlatform(RHIName);
	switch(Platform)
	{
		case SP_PCD3D_SM5:
			FriendlyRHIName = LOCTEXT("D3DSM5", "Direct3D 11+ (SM5)");
			break;
		case SP_PCD3D_ES3_1:
			FriendlyRHIName = LOCTEXT("D3DES31", "Direct3D (ES3.1, Mobile Preview)");
			break;
		case SP_OPENGL_PCES3_1:
			FriendlyRHIName = LOCTEXT("OpenGLES31PC", "OpenGL (ES3.1, Mobile Preview)");
			break;
		case SP_OPENGL_ES3_1_ANDROID:
			FriendlyRHIName = LOCTEXT("OpenGLES31", "OpenGLES 3.1 (Mobile)");
			break;
		case SP_METAL:
			FriendlyRHIName = LOCTEXT("Metal", "iOS Metal Mobile Renderer (ES3.1, Metal 1.1+, iOS 9.0 or later)");
			break;
		case SP_METAL_MRT:
			FriendlyRHIName = LOCTEXT("MetalMRT", "iOS Metal Desktop Renderer (SM5, Metal 1.2+, iOS 10.0 or later)");
			break;
		case SP_METAL_TVOS:
			FriendlyRHIName = LOCTEXT("MetalTV", "tvOS Metal Mobile Renderer (ES3.1, Metal 1.1+, tvOS 9.0 or later)");
			break;
		case SP_METAL_MRT_TVOS:
			FriendlyRHIName = LOCTEXT("MetalMRTTV", "tvOS Metal Desktop Renderer (SM5, Metal 1.2+, tvOS 10.0 or later)");
			break;
		case SP_METAL_SM5_NOTESS:
			FriendlyRHIName = LOCTEXT("MetalSM5_NOTESS", "Mac Metal Desktop Renderer without Tessellation (SM5, Metal 2.0+, macOS High Sierra 10.13.6 or later)");
			break;
		case SP_METAL_SM5:
			FriendlyRHIName = LOCTEXT("MetalSM5", "Mac Metal Desktop Renderer with Tessellation (SM5, Metal 2.0+, macOS High Sierra 10.13.6 or later)");
			break;
		case SP_METAL_MACES3_1:
			FriendlyRHIName = LOCTEXT("MetalES3.1", "Mac Metal High-End Mobile Preview (ES3.1)");
			break;
		case SP_METAL_MRT_MAC:
			FriendlyRHIName = LOCTEXT("MetalMRTMac", "Mac Metal iOS/tvOS Desktop Renderer Preview (SM5)");
			break;
		case SP_VULKAN_SM5:
		case SP_VULKAN_SM5_LUMIN:
		case SP_VULKAN_SM5_ANDROID:
			FriendlyRHIName = LOCTEXT("VulkanSM5", "Vulkan (SM5)");
			break;
		case SP_VULKAN_PCES3_1:
		case SP_VULKAN_ES3_1_ANDROID:
		case SP_VULKAN_ES3_1_LUMIN:
			FriendlyRHIName = LOCTEXT("VulkanES31", "Vulkan (ES 3.1)");
			break;	
		default:
			break;
	}
	
	return FriendlyRHIName;
}

FShaderFormatsPropertyDetails::FShaderFormatsPropertyDetails(IDetailLayoutBuilder* InDetailBuilder, FString InProperty, FString InTitle)
: DetailBuilder(InDetailBuilder)
, Property(InProperty)
, Title(InTitle)
{
	ShaderFormatsPropertyHandle = DetailBuilder->GetProperty(*Property);
	ensure(ShaderFormatsPropertyHandle.IsValid());
}

void FShaderFormatsPropertyDetails::SetOnUpdateShaderWarning(FSimpleDelegate const& Delegate)
{
	ShaderFormatsPropertyHandle->SetOnPropertyValueChanged(Delegate);
}

void FShaderFormatsPropertyDetails::CreateTargetShaderFormatsPropertyView(ITargetPlatform* TargetPlatform, GetFriendlyNameFromRHINameFnc FriendlyNameFnc)
{
	check(TargetPlatform);
	DetailBuilder->HideProperty(ShaderFormatsPropertyHandle);
	
	// List of supported RHI's and selected targets
	TArray<FName> ShaderFormats;
	TargetPlatform->GetAllPossibleShaderFormats(ShaderFormats);
	
	IDetailCategoryBuilder& TargetedRHICategoryBuilder = DetailBuilder->EditCategory(*Title);
	
	int32 ShaderCounter = 0;
	for (const FName& ShaderFormat : ShaderFormats)
	{
		FText FriendlyShaderFormatName = FriendlyNameFnc(ShaderFormat.ToString());
		if (!FriendlyShaderFormatName.IsEmpty())
		{
			ShaderFormatOrder.Add(ShaderFormat, ShaderCounter++);
			FDetailWidgetRow& TargetedRHIWidgetRow = TargetedRHICategoryBuilder.AddCustomRow(FriendlyShaderFormatName);

			TargetedRHIWidgetRow
			.NameContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(0, 1, 0, 1))
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(FriendlyShaderFormatName)
					.Font(DetailBuilder->GetDetailFont())
				]
			]
			.ValueContent()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &FShaderFormatsPropertyDetails::OnTargetedRHIChanged, ShaderFormat)
				.IsChecked(this, &FShaderFormatsPropertyDetails::IsTargetedRHIChecked, ShaderFormat)
			];
		}
	}
}


void FShaderFormatsPropertyDetails::OnTargetedRHIChanged(ECheckBoxState InNewValue, FName InRHIName)
{
	TArray<void*> RawPtrs;
	ShaderFormatsPropertyHandle->AccessRawData(RawPtrs);
	
	// Update the CVars with the selection
	{
		ShaderFormatsPropertyHandle->NotifyPreChange();
		for (void* RawPtr : RawPtrs)
		{
			TArray<FString>& Array = *(TArray<FString>*)RawPtr;
			if(InNewValue == ECheckBoxState::Checked)
			{
				// Preserve order from GetAllPossibleShaderFormats
				const int32 InIndex = ShaderFormatOrder[InRHIName];
				int32 InsertIndex = 0;
				for (; InsertIndex < Array.Num(); ++InsertIndex)
				{
					const int32* ShaderFormatIndex = ShaderFormatOrder.Find(*Array[InsertIndex]);
					if (ShaderFormatIndex != nullptr && InIndex < *ShaderFormatIndex) 
					{
						break;
					}
				}
				Array.Insert(InRHIName.ToString(), InsertIndex);
			}
			else
			{
				Array.Remove(InRHIName.ToString());
			}
		}

		ShaderFormatsPropertyHandle->NotifyPostChange(EPropertyChangeType::Unspecified);
	}
}


ECheckBoxState FShaderFormatsPropertyDetails::IsTargetedRHIChecked(FName InRHIName) const
{
	ECheckBoxState CheckState = ECheckBoxState::Unchecked;
	
	TArray<void*> RawPtrs;
	ShaderFormatsPropertyHandle->AccessRawData(RawPtrs);
	
	for(void* RawPtr : RawPtrs)
	{
		TArray<FString>& Array = *(TArray<FString>*)RawPtr;
		if(Array.Contains(InRHIName.ToString()))
		{
			CheckState = ECheckBoxState::Checked;
		}
	}
	return CheckState;
}

#undef LOCTEXT_NAMESPACE
