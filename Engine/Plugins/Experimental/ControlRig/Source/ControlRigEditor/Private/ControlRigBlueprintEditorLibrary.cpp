// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintEditorLibrary.h"

void UControlRigBlueprintEditorLibrary::CastToControlRigBlueprint(
	UObject* Object,
	ECastToControlRigBlueprintCases& Branches,
	UControlRigBlueprint*& AsControlRigBlueprint)
{
	AsControlRigBlueprint = Cast<UControlRigBlueprint>(Object);
	Branches = AsControlRigBlueprint == nullptr ? 
		ECastToControlRigBlueprintCases::CastFailed : 
		ECastToControlRigBlueprintCases::CastSucceeded;
}

void UControlRigBlueprintEditorLibrary::SetPreviewMesh(UControlRigBlueprint* InRigBlueprint, USkeletalMesh* PreviewMesh, bool bMarkAsDirty)
{
	if(InRigBlueprint == nullptr)
	{
		return;
	}
	InRigBlueprint->SetPreviewMesh(PreviewMesh, bMarkAsDirty);
}

USkeletalMesh* UControlRigBlueprintEditorLibrary::GetPreviewMesh(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return nullptr;
	}
	return InRigBlueprint->GetPreviewMesh();
}

void UControlRigBlueprintEditorLibrary::RecompileVM(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return;
	}
	InRigBlueprint->RecompileVM();
}

void UControlRigBlueprintEditorLibrary::RecompileVMIfRequired(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return;
	}
	InRigBlueprint->RecompileVMIfRequired();
}

void UControlRigBlueprintEditorLibrary::RequestAutoVMRecompilation(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return;
	}
	InRigBlueprint->RequestAutoVMRecompilation();
}

void UControlRigBlueprintEditorLibrary::RequestControlRigInit(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return;
	}
	InRigBlueprint->RequestControlRigInit();
}

URigVMGraph* UControlRigBlueprintEditorLibrary::GetModel(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return nullptr;
	}
	return InRigBlueprint->GetModel();
}

URigVMController* UControlRigBlueprintEditorLibrary::GetController(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return nullptr;
	}
	return InRigBlueprint->GetController();
}

TArray<UControlRigBlueprint*> UControlRigBlueprintEditorLibrary::GetCurrentlyOpenRigBlueprints()
{
	return UControlRigBlueprint::GetCurrentlyOpenRigBlueprints();
}

TArray<UStruct*> UControlRigBlueprintEditorLibrary::GetAvailableRigUnits()
{
	return UControlRigBlueprint::GetAvailableRigUnits();
}

UControlRigHierarchyModifier* UControlRigBlueprintEditorLibrary::GetHierarchyModifier(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return nullptr;
	}
	return InRigBlueprint->GetHierarchyModifier();
}
