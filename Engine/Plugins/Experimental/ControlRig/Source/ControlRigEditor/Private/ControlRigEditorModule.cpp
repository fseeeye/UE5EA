// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigEditorModule.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintEditorModule.h"
#include "BlueprintNodeSpawner.h"
#include "PropertyEditorModule.h"
#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "ControlRigConnectionDrawingPolicy.h"
#include "ControlRigVariableDetailsCustomization.h"
#include "GraphEditorActions.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ISequencerModule.h"
#include "IAssetTools.h"
#include "ControlRigEditorStyle.h"
#include "Framework/Docking/LayoutExtender.h"
#include "Framework/Application/SlateApplication.h"
#include "LevelEditor.h"
#include "MovieSceneToolsProjectSettings.h"
#include "Styling/SlateStyle.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "EditorModeRegistry.h"
#include "ControlRigEditMode.h"
#include "ILevelSequenceModule.h"
#include "EditorModeManager.h"
#include "ControlRigEditMode.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "MovieSceneControlRigSectionDetailsCustomization.h"
#include "ControlRigEditModeCommands.h"
#include "Materials/Material.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "ContentBrowserMenuContexts.h"
#include "ControlRigEditModeSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "Editor/ControlRigEditor.h"
#include "ControlRigBlueprintActions.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "ControlRigGizmoLibraryActions.h"
#include "Graph/ControlRigGraphSchema.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/NodeSpawners/ControlRigUnitNodeSpawner.h"
#include "Graph/NodeSpawners/ControlRigVariableNodeSpawner.h"
#include "Graph/NodeSpawners/ControlRigRerouteNodeSpawner.h"
#include "Graph/NodeSpawners/ControlRigBranchNodeSpawner.h"
#include "Graph/NodeSpawners/ControlRigIfNodeSpawner.h"
#include "Graph/NodeSpawners/ControlRigSelectNodeSpawner.h"
#include "Graph/NodeSpawners/ControlRigPrototypeNodeSpawner.h"
#include "Graph/NodeSpawners/ControlRigEnumNodeSpawner.h"
#include "Graph/NodeSpawners/ControlRigFunctionRefNodeSpawner.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Graph/ControlRigGraphNode.h"
#include "EdGraphUtilities.h"
#include "ControlRigGraphPanelNodeFactory.h"
#include "ControlRigGraphPanelPinFactory.h"
#include "ControlRigBlueprintUtils.h"
#include "ControlRigBlueprintCommands.h"
#include "ControlRigHierarchyCommands.h"
#include "ControlRigStackCommands.h"
#include "Animation/AnimSequence.h"
#include "ControlRigEditorEditMode.h"
#include "ControlRigElementDetails.h"
#include "ControlRigCompilerDetails.h"
#include "ControlRigDrawingDetails.h"
#include "ControlRigGraphDetails.h"
#include "ControlRigInfluenceMapDetails.h"
#include "Animation/AnimSequence.h"
#include "Editor/SControlRigProfilingView.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ControlRigParameterTrackEditor.h"
#include "ActorFactories/ActorFactorySkeletalMesh.h"
#include "ControlRigThumbnailRenderer.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/Nodes/RigVMLibraryNode.h"
#include "ControlRigBlueprint.h"
#include "ControlRig/Private/Units/Simulation/RigUnit_AlphaInterp.h"
#include "ControlRig/Private/Units/Debug/RigUnit_VisualDebug.h"
#include "SKismetInspector.h"
#include "Dialogs/Dialogs.h"
#include "Settings/ControlRigSettings.h"
#include "EditMode/ControlRigControlsProxy.h"
#include "IPersonaToolkit.h"
#include "LevelSequence.h"
#include "AnimSequenceLevelSequenceLink.h"
#include "LevelSequenceActor.h"
#include "ISequencer.h"
#include "ILevelSequenceEditorToolkit.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "Animation/SkeletalMeshActor.h"
#include "ControlRig.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "ControlRigObjectBinding.h"
#include "EditMode/ControlRigEditMode.h"
#include "LevelSequenceAnimSequenceLink.h"
#include "AnimSequenceLevelSequenceLink.h"
#include "Rigs/FKControlRig.h"
#include "SBakeToControlRigDialog.h"
#include "ControlRig/Private/Units/Execution/RigUnit_InverseExecution.h"
#include "Graph/SControlRigGraphPinVariableBinding.h"
#include "AssetTypeActions_ControlRigPose.h"
#include "EditMode/SControlRigBaseListWidget.h"
#include "EditMode/SControlRigTweenWidget.h"
#include "EditMode/SControlRigSnapper.h"

#define LOCTEXT_NAMESPACE "ControlRigEditorModule"

DEFINE_LOG_CATEGORY(LogControlRigEditor);

const FName IControlRigEditorModule::ControlRigPoseTab = FName("ControlRigPoseTab");
const FName IControlRigEditorModule::ControlRigTweenTab = FName("ControlRigTweenTab");
const FName IControlRigEditorModule::ControlRigSnapperTab = FName("ControlRigSnapperTab");
const FName IControlRigEditorModule::ControlRigTempPivotTab = FName("ControlRigTempPivotTab");


TSharedRef<SDockTab> SpawnRigProfiler( const FSpawnTabArgs& Args )
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SControlRigProfilingView)
		];
}

TSharedRef<SDockTab> SpawnPoseTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SControlRigBaseListWidget)
		];
}

TSharedRef<SDockTab> SpawnTweenTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SControlRigTweenWidget)
		];
}

TSharedRef<SDockTab> SpawnSnapperTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SControlRigSnapper)
		];
}

TSharedRef<SDockTab> SpawnPivotTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SControlRigTweenWidget)
		];
}

void FControlRigEditorModule::StartupModule()
{
	FControlRigEditModeCommands::Register();
	FControlRigBlueprintCommands::Register();
	FControlRigHierarchyCommands::Register();
	FControlRigStackCommands::Register();
	FControlRigEditorStyle::Get();

	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	// Register Blueprint editor variable customization
	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
	BlueprintEditorModule.RegisterVariableCustomization(FProperty::StaticClass(), FOnGetVariableCustomizationInstance::CreateStatic(&FControlRigVariableDetailsCustomization::MakeInstance));
	BlueprintEditorModule.RegisterGraphCustomization(GetDefault<UControlRigGraphSchema>(), FOnGetGraphCustomizationInstance::CreateStatic(&FControlRigGraphDetails::MakeInstance));

	// Register to fixup newly created BPs
	FKismetEditorUtilities::RegisterOnBlueprintCreatedCallback(this, UControlRig::StaticClass(), FKismetEditorUtilities::FOnBlueprintCreated::CreateRaw(this, &FControlRigEditorModule::HandleNewBlueprintCreated));

	// Register details customizations for animation controller nodes
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	ClassesToUnregisterOnShutdown.Reset();

	ClassesToUnregisterOnShutdown.Add(UMovieSceneControlRigParameterSection::StaticClass()->GetFName());
	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown.Last(), FOnGetDetailCustomizationInstance::CreateStatic(&FMovieSceneControlRigSectionDetailsCustomization::MakeInstance));

	ClassesToUnregisterOnShutdown.Add(FRigBone::StaticStruct()->GetFName());
	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown.Last(), FOnGetDetailCustomizationInstance::CreateStatic(&FRigBoneDetails::MakeInstance));

	ClassesToUnregisterOnShutdown.Add(FRigControl::StaticStruct()->GetFName());
	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown.Last(), FOnGetDetailCustomizationInstance::CreateStatic(&FRigControlDetails::MakeInstance));

	ClassesToUnregisterOnShutdown.Add(FRigSpace::StaticStruct()->GetFName());
	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown.Last(), FOnGetDetailCustomizationInstance::CreateStatic(&FRigSpaceDetails::MakeInstance));

	ClassesToUnregisterOnShutdown.Add(FRigInfluenceMapPerEvent::StaticStruct()->GetFName());
	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown.Last(), FOnGetDetailCustomizationInstance::CreateStatic(&FRigInfluenceMapPerEventDetails::MakeInstance));

	ClassesToUnregisterOnShutdown.Add(UControlRig::StaticClass()->GetFName());

	// same as ClassesToUnregisterOnShutdown but for properties, there is none right now
	PropertiesToUnregisterOnShutdown.Reset();

	PropertiesToUnregisterOnShutdown.Add(FRigVMCompileSettings::StaticStruct()->GetFName());
	PropertyEditorModule.RegisterCustomPropertyTypeLayout(PropertiesToUnregisterOnShutdown.Last(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRigVMCompileSettingsDetails::MakeInstance));

	PropertiesToUnregisterOnShutdown.Add(FControlRigDrawContainer::StaticStruct()->GetFName());
	PropertyEditorModule.RegisterCustomPropertyTypeLayout(PropertiesToUnregisterOnShutdown.Last(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FControlRigDrawContainerDetails::MakeInstance));

	PropertiesToUnregisterOnShutdown.Add(FControlRigEnumControlProxyValue::StaticStruct()->GetFName());
	PropertyEditorModule.RegisterCustomPropertyTypeLayout(PropertiesToUnregisterOnShutdown.Last(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FControlRigEnumControlProxyValueDetails::MakeInstance));

	PropertiesToUnregisterOnShutdown.Add(FRigElementKey::StaticStruct()->GetFName());
	PropertyEditorModule.RegisterCustomPropertyTypeLayout(PropertiesToUnregisterOnShutdown.Last(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRigElementKeyDetails::MakeInstance));

	// Register asset tools
	auto RegisterAssetTypeAction = [this](const TSharedRef<IAssetTypeActions>& InAssetTypeAction)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		RegisteredAssetTypeActions.Add(InAssetTypeAction);
		AssetTools.RegisterAssetTypeActions(InAssetTypeAction);
	};

	RegisterAssetTypeAction(MakeShareable(new FControlRigBlueprintActions()));
	RegisterAssetTypeAction(MakeShareable(new FControlRigGizmoLibraryActions()));
	RegisterAssetTypeAction(MakeShareable(new FAssetTypeActions_ControlRigPose()));
	
	// Register sequencer track editor
	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	ControlRigParameterTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FControlRigParameterTrackEditor::CreateTrackEditor));

	//Register Animation Toolbar Extender
	IAnimationEditorModule& AnimationEditorModule = FModuleManager::Get().LoadModuleChecked<IAnimationEditorModule>("AnimationEditor");
	auto& ToolbarExtenders = AnimationEditorModule.GetAllAnimationEditorToolbarExtenders();

	ToolbarExtenders.Add(IAnimationEditorModule::FAnimationEditorToolbarExtender::CreateRaw(this, &FControlRigEditorModule::GetAnimationEditorToolbarExtender));
	AnimationEditorExtenderHandle = ToolbarExtenders.Last().GetHandle();

	FEditorModeRegistry::Get().RegisterMode<FControlRigEditMode>(
		FControlRigEditMode::ModeName,
		NSLOCTEXT("AnimationModeToolkit", "DisplayName", "Animation"),
		FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRigEditMode", "ControlRigEditMode.Small"),
		true);

	FEditorModeRegistry::Get().RegisterMode<FControlRigEditorEditMode>(
		FControlRigEditorEditMode::ModeName,
		NSLOCTEXT("RiggingModeToolkit", "DisplayName", "Rigging"),
		FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRigEditMode", "ControlRigEditMode.Small"),
		false);


	ControlRigGraphPanelNodeFactory = MakeShared<FControlRigGraphPanelNodeFactory>();
	FEdGraphUtilities::RegisterVisualNodeFactory(ControlRigGraphPanelNodeFactory);

	ControlRigGraphPanelPinFactory = MakeShared<FControlRigGraphPanelPinFactory>();
	FEdGraphUtilities::RegisterVisualPinFactory(ControlRigGraphPanelPinFactory);

	ReconstructAllNodesDelegateHandle = FBlueprintEditorUtils::OnReconstructAllNodesEvent.AddStatic(&FControlRigBlueprintUtils::HandleReconstructAllNodes);
	RefreshAllNodesDelegateHandle = FBlueprintEditorUtils::OnRefreshAllNodesEvent.AddStatic(&FControlRigBlueprintUtils::HandleRefreshAllNodes);

#if WITH_EDITOR
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner("HierarchicalProfiler", FOnSpawnTab::CreateStatic(&SpawnRigProfiler))
			.SetDisplayName(NSLOCTEXT("UnrealEditor", "HierarchicalProfilerTab", "Hierarchical Profiler"))
			.SetTooltipText(NSLOCTEXT("UnrealEditor", "HierarchicalProfilerTooltip", "Open the Hierarchical Profiler tab."))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsProfilingCategory())
			.SetIcon(FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("HierarchicalProfiler.TabIcon")));

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(IControlRigEditorModule::ControlRigPoseTab, FOnSpawnTab::CreateStatic(&SpawnPoseTab))
			.SetDisplayName(NSLOCTEXT("UnrealEditor", "ControlRigPoseTab", "Control Rig Pose"))
			.SetTooltipText(NSLOCTEXT("UnrealEditor", "ControlRigPoseTabTooltip", "Open the Control Rig Pose tab."))
			.SetMenuType(ETabSpawnerMenuType::Hidden)
			.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "FoliageEditMode.Settings"));//MZ todo replace with correct icon
		FGlobalTabmanager::Get()->RegisterDefaultTabWindowSize(IControlRigEditorModule::ControlRigPoseTab, FVector2D(850, 800));
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(IControlRigEditorModule::ControlRigTweenTab, FOnSpawnTab::CreateStatic(&SpawnTweenTab))
			.SetDisplayName(NSLOCTEXT("UnrealEditor", "ControlRigTweenTab", "Control Rig Tween"))
			.SetTooltipText(NSLOCTEXT("UnrealEditor", "ControlRigTweenTooltip", "Open the Control Rig Tween tab."))
			.SetMenuType(ETabSpawnerMenuType::Hidden)
			.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "SkeletonTree.BlendProfile")); //MZ todo replace with correct icon
		FGlobalTabmanager::Get()->RegisterDefaultTabWindowSize(IControlRigEditorModule::ControlRigTweenTab, FVector2D(400, 100));

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(IControlRigEditorModule::ControlRigSnapperTab, FOnSpawnTab::CreateStatic(&SpawnSnapperTab))
			.SetDisplayName(NSLOCTEXT("UnrealEditor", "ControlRigSnapperTab", "Control Rig Snapper"))
			.SetTooltipText(NSLOCTEXT("UnrealEditor", "ControlRigSnapperTabTooltip", "Open the Control Rig Snapper tab."))
			.SetMenuType(ETabSpawnerMenuType::Hidden)
			.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.Tabs.AnimSlotManager"));//MZ todo replace with correct icon SkeletonTree.SkeletonSocket
		FGlobalTabmanager::Get()->RegisterDefaultTabWindowSize(IControlRigEditorModule::ControlRigSnapperTab, FVector2D(400, 400));

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(IControlRigEditorModule::ControlRigTempPivotTab, FOnSpawnTab::CreateStatic(&SpawnPivotTab))
			.SetDisplayName(NSLOCTEXT("UnrealEditor", "ControlRigTempPivotTab", "Control Rig Temp Pivot Tab"))
			.SetTooltipText(NSLOCTEXT("UnrealEditor", "ControlRigTweenTooltip", "Open the Control Rig Pivot tab."))
			.SetMenuType(ETabSpawnerMenuType::Hidden)
			.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "SkeletonTree.BlendProfile")); //MZ todo replace with correct icon SkeletonTree.LODBone
		FGlobalTabmanager::Get()->RegisterDefaultTabWindowSize(IControlRigEditorModule::ControlRigTempPivotTab, FVector2D(400, 100));


	};
#endif

	FControlRigBlueprintActions::ExtendSketalMeshToolMenu();
	ExtendAnimSequenceMenu();

	UActorFactorySkeletalMesh::RegisterDelegatesForAssetClass(
		UControlRigBlueprint::StaticClass(),
		FGetSkeletalMeshFromAssetDelegate::CreateStatic(&FControlRigBlueprintActions::GetSkeletalMeshFromControlRigBlueprint),
		FPostSkeletalMeshActorSpawnedDelegate::CreateStatic(&FControlRigBlueprintActions::PostSpawningSkeletalMeshActor)
	);

	UThumbnailManager::Get().RegisterCustomRenderer(UControlRigBlueprint::StaticClass(), UControlRigThumbnailRenderer::StaticClass());
	//UThumbnailManager::Get().RegisterCustomRenderer(UControlRigPoseAsset::StaticClass(), UControlRigPoseThumbnailRenderer::StaticClass());

	bFilterAssetBySkeleton = true;
}

void FControlRigEditorModule::ShutdownModule()
{
#if WITH_EDITOR
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("ControlRigProfiler");
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(IControlRigEditorModule::ControlRigPoseTab);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(IControlRigEditorModule::ControlRigTweenTab);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(IControlRigEditorModule::ControlRigSnapperTab);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(IControlRigEditorModule::ControlRigTempPivotTab);
	}
#endif

	//UThumbnailManager::Get().UnregisterCustomRenderer(UControlRigBlueprint::StaticClass());
	//UActorFactorySkeletalMesh::UnregisterDelegatesForAssetClass(UControlRigBlueprint::StaticClass());

	FBlueprintEditorUtils::OnRefreshAllNodesEvent.Remove(RefreshAllNodesDelegateHandle);
	FBlueprintEditorUtils::OnReconstructAllNodesEvent.Remove(ReconstructAllNodesDelegateHandle);

	FEdGraphUtilities::UnregisterVisualPinFactory(ControlRigGraphPanelPinFactory);
	FEdGraphUtilities::UnregisterVisualNodeFactory(ControlRigGraphPanelNodeFactory);

	FEditorModeRegistry::Get().UnregisterMode(FControlRigEditorEditMode::ModeName);
	FEditorModeRegistry::Get().UnregisterMode(FControlRigEditMode::ModeName);


	ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>("Sequencer");
	if (SequencerModule)
	{
		SequencerModule->UnRegisterTrackEditor(ControlRigParameterTrackCreateEditorHandle);
	}

	FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");
	if (AssetToolsModule)
	{
		for (TSharedRef<IAssetTypeActions> RegisteredAssetTypeAction : RegisteredAssetTypeActions)
		{
			AssetToolsModule->Get().UnregisterAssetTypeActions(RegisteredAssetTypeAction);
		}
	}

	FKismetEditorUtilities::UnregisterAutoBlueprintNodeCreation(this);

	if (!IsEngineExitRequested())
	{
		FBlueprintEditorModule* BlueprintEditorModule = FModuleManager::GetModulePtr<FBlueprintEditorModule>("Kismet");
		if (BlueprintEditorModule)
		{
			BlueprintEditorModule->UnregisterVariableCustomization(FProperty::StaticClass());
			BlueprintEditorModule->UnregisterGraphCustomization(GetDefault<UControlRigGraphSchema>());
		}
	}

	FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyEditorModule)
	{
		for (int32 Index = 0; Index < ClassesToUnregisterOnShutdown.Num(); ++Index)
		{
			PropertyEditorModule->UnregisterCustomClassLayout(ClassesToUnregisterOnShutdown[Index]);
		}

		for (int32 Index = 0; Index < PropertiesToUnregisterOnShutdown.Num(); ++Index)
		{
			PropertyEditorModule->UnregisterCustomPropertyTypeLayout(PropertiesToUnregisterOnShutdown[Index]);
		}
	}

	IAnimationEditorModule* AnimationEditorModule = FModuleManager::Get().GetModulePtr<IAnimationEditorModule>("AnimationEditor");
	if (AnimationEditorModule)
	{
		typedef IAnimationEditorModule::FAnimationEditorToolbarExtender DelegateType;
		AnimationEditorModule->GetAllAnimationEditorToolbarExtenders().RemoveAll([=](const DelegateType& In) { return In.GetHandle() == AnimationEditorExtenderHandle; });
	}
}

TSharedRef<FExtender> FControlRigEditorModule::GetAnimationEditorToolbarExtender(const TSharedRef<FUICommandList> CommandList, TSharedRef<IAnimationEditor> InAnimationEditor)
{
	TSharedRef<FExtender> Extender = MakeShareable(new FExtender);

	USkeleton* Skeleton = InAnimationEditor->GetPersonaToolkit()->GetSkeleton();
	USkeletalMesh* SkeletalMesh = InAnimationEditor->GetPersonaToolkit()->GetPreviewMesh();
	if (!SkeletalMesh) //if no preview mesh just get normal mesh
	{
		SkeletalMesh = InAnimationEditor->GetPersonaToolkit()->GetMesh();
	}
	if (Skeleton && SkeletalMesh)
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(InAnimationEditor->GetPersonaToolkit()->GetAnimationAsset());
		if (AnimSequence)
		{
			Extender->AddToolBarExtension(
				"Asset",
				EExtensionHook::After,
				CommandList,
				FToolBarExtensionDelegate::CreateRaw(this, &FControlRigEditorModule::HandleAddControlRigExtenderToToolbar, TWeakPtr<IAnimationEditor>(InAnimationEditor))
			);
		}
	}

	return Extender;
}


TSharedRef< SWidget > FControlRigEditorModule::GenerateAnimationMenu(TWeakPtr<IAnimationEditor> InAnimationEditor)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);
	
	if(InAnimationEditor.IsValid())
	{
		TSharedRef<IAnimationEditor> AnimationEditor = InAnimationEditor.Pin().ToSharedRef();
		USkeleton* Skeleton = AnimationEditor->GetPersonaToolkit()->GetSkeleton();
		USkeletalMesh* SkeletalMesh = AnimationEditor->GetPersonaToolkit()->GetPreviewMesh();
		if (!SkeletalMesh) //if no preview mesh just get normal mesh
		{
			SkeletalMesh = AnimationEditor->GetPersonaToolkit()->GetMesh();
		}
		
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimationEditor->GetPersonaToolkit()->GetAnimationAsset());
		if (Skeleton && SkeletalMesh && AnimSequence)
		{
			FUIAction EditWithFKControlRig(
				FExecuteAction::CreateRaw(this, &FControlRigEditorModule::EditWithFKControlRig, AnimSequence, SkeletalMesh, Skeleton));

			FUIAction OpenIt(
				FExecuteAction::CreateStatic(&FControlRigEditorModule::OpenLevelSequence, AnimSequence),
				FCanExecuteAction::CreateLambda([AnimSequence]()
					{
						if (AnimSequence)
						{
							if (IInterface_AssetUserData* AnimAssetUserData = Cast< IInterface_AssetUserData >(AnimSequence))
							{
								UAnimSequenceLevelSequenceLink* AnimLevelLink = AnimAssetUserData->GetAssetUserData< UAnimSequenceLevelSequenceLink >();
								if (AnimLevelLink)
								{
									ULevelSequence* LevelSequence = AnimLevelLink->ResolveLevelSequence();
									if (LevelSequence)
									{
										return true;
									}
								}
							}
						}
						return false;
					}
				)

			);

			FUIAction UnLinkIt(
				FExecuteAction::CreateStatic(&FControlRigEditorModule::UnLinkLevelSequence, AnimSequence),
				FCanExecuteAction::CreateLambda([AnimSequence]()
					{
						if (AnimSequence)
						{
							if (IInterface_AssetUserData* AnimAssetUserData = Cast< IInterface_AssetUserData >(AnimSequence))
							{
								UAnimSequenceLevelSequenceLink* AnimLevelLink = AnimAssetUserData->GetAssetUserData< UAnimSequenceLevelSequenceLink >();
								if (AnimLevelLink)
								{
									ULevelSequence* LevelSequence = AnimLevelLink->ResolveLevelSequence();
									if (LevelSequence)
									{
										return true;
									}
								}
							}
						}
						return false;
					}
				)

			);

			FUIAction ToggleFilterAssetBySkeleton(
				FExecuteAction::CreateLambda([this]()
					{
						bFilterAssetBySkeleton = bFilterAssetBySkeleton ? false : true;
					}
				),
				FCanExecuteAction(),
						FIsActionChecked::CreateLambda([this]()
							{
								return bFilterAssetBySkeleton;
							}
						)
						);
			if (Skeleton)
			{
				MenuBuilder.BeginSection("Control Rig", LOCTEXT("ControlRig", "Control Rig"));
				{
					MenuBuilder.AddMenuEntry(LOCTEXT("EditWithFKControlRig", "Edit With FK Control Rig"),
						FText(), FSlateIcon(), EditWithFKControlRig, NAME_None, EUserInterfaceActionType::Button);


					MenuBuilder.AddMenuEntry(
						LOCTEXT("FilterAssetBySkeleton", "Filter Asset By Skeleton"),
						LOCTEXT("FilterAssetBySkeletonTooltip", "Filters Control Rig Assets To Match Current Skeleton"),
						FSlateIcon(),
						ToggleFilterAssetBySkeleton,
						NAME_None,
						EUserInterfaceActionType::ToggleButton);

					MenuBuilder.AddSubMenu(
						LOCTEXT("BakeToControlRig", "Bake To Control Rig"), NSLOCTEXT("AnimationModeToolkit", "BakeToControlRigTooltip", "This Control Rig will Drive This Animation."),
						FNewMenuDelegate::CreateLambda([this, AnimSequence, SkeletalMesh, Skeleton](FMenuBuilder& InSubMenuBuilder)
							{
								//todo move to .h for ue5
								class FControlRigClassFilter : public IClassViewerFilter
								{
								public:
									FControlRigClassFilter(bool bInCheckSkeleton, bool bInCheckAnimatable, bool bInCheckInversion, USkeleton* InSkeleton) :
										bFilterAssetBySkeleton(bInCheckSkeleton),
										bFilterExposesAnimatableControls(bInCheckAnimatable),
										bFilterInversion(bInCheckInversion),
										AssetRegistry(FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get())
									{
										if (InSkeleton)
										{
											SkeletonName = FAssetData(InSkeleton).GetExportTextName();
										}
									}
									bool bFilterAssetBySkeleton;
									bool bFilterExposesAnimatableControls;
									bool bFilterInversion;

									FString SkeletonName;
									const IAssetRegistry& AssetRegistry;

									bool MatchesFilter(const FAssetData& AssetData)
									{
										bool bExposesAnimatableControls = AssetData.GetTagValueRef<bool>(TEXT("bExposesAnimatableControls"));
										if (bFilterExposesAnimatableControls == true && bExposesAnimatableControls == false)
										{
											return false;
										}
										if (bFilterInversion)
										{
											bool bHasInversion = false;
											FAssetDataTagMapSharedView::FFindTagResult Tag = AssetData.TagsAndValues.FindTag(TEXT("SupportedEventNames"));
											if (Tag.IsSet())
											{
												FString EventString = FRigUnit_InverseExecution::EventName.ToString();
												TArray<FString> SupportedEventNames;
												Tag.GetValue().ParseIntoArray(SupportedEventNames, TEXT(","), true);

												for (const FString& Name : SupportedEventNames)
												{
													if (Name.Contains(EventString))
													{
														bHasInversion = true;
														break;
													}
												}
												if (bHasInversion == false)
												{
													return false;
												}
											}
										}
										if (bFilterAssetBySkeleton)
										{
											FString PreviewSkeletalMesh = AssetData.GetTagValueRef<FString>(TEXT("PreviewSkeletalMesh"));
											if (PreviewSkeletalMesh.Len() > 0)
											{
												FAssetData SkelMeshData = AssetRegistry.GetAssetByObjectPath(FName(*PreviewSkeletalMesh));
												FString PreviewSkeleton = SkelMeshData.GetTagValueRef<FString>(TEXT("Skeleton"));
												if (PreviewSkeleton == SkeletonName)
												{
													return true;
												}
											}
											FString PreviewSkeleton = AssetData.GetTagValueRef<FString>(TEXT("PreviewSkeleton"));
											if (PreviewSkeleton == SkeletonName)
											{
												return true;
											}
											FString SourceHierarchyImport = AssetData.GetTagValueRef<FString>(TEXT("SourceHierarchyImport"));
											if (SourceHierarchyImport == SkeletonName)
											{
												return true;
											}
											FString SourceCurveImport = AssetData.GetTagValueRef<FString>(TEXT("SourceCurveImport"));
											if (SourceCurveImport == SkeletonName)
											{
												return true;
											}
											return false;
										}
										return true;

									}
									bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
									{
										const bool bChildOfObjectClass = InClass->IsChildOf(UControlRig::StaticClass());
										const bool bMatchesFlags = !InClass->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract);
										const bool bNotNative = !InClass->IsNative();

										if (bChildOfObjectClass && bMatchesFlags && bNotNative)
										{
											FAssetData AssetData(InClass);
											return MatchesFilter(AssetData);

										}
										return false;
									}

									virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
									{
										const bool bChildOfObjectClass = InUnloadedClassData->IsChildOf(UControlRig::StaticClass());
										const bool bMatchesFlags = !InUnloadedClassData->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract);
										if (bChildOfObjectClass && bMatchesFlags)
										{
											FString GeneratedClassPathString = InUnloadedClassData->GetClassPath().ToString();
											FName BlueprintPath = FName(*GeneratedClassPathString.LeftChop(2)); // Chop off _C
											FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(BlueprintPath);
											return MatchesFilter(AssetData);

										}
										return false;
									}

								};

								FClassViewerInitializationOptions Options;
								Options.bShowUnloadedBlueprints = true;
								Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;

								TSharedPtr<FControlRigClassFilter> ClassFilter = MakeShareable(new FControlRigClassFilter(bFilterAssetBySkeleton, true, true, Skeleton));
								Options.ClassFilter = ClassFilter;
								Options.bShowNoneOption = false;

								FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

								TSharedRef<SWidget> ClassViewer = ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateRaw(this, &FControlRigEditorModule::BakeToControlRig, AnimSequence, SkeletalMesh, Skeleton));
								InSubMenuBuilder.AddWidget(ClassViewer, FText::GetEmpty(), true);
							})
					);
				}
				MenuBuilder.EndSection();

			}

			MenuBuilder.AddMenuEntry(LOCTEXT("OpenLevelSequence", "Open Level Sequence"),
				FText(), FSlateIcon(), OpenIt, NAME_None, EUserInterfaceActionType::Button);

			MenuBuilder.AddMenuEntry(LOCTEXT("UnlinkLevelSequence", "Unlink Level Sequence"),
				FText(), FSlateIcon(), UnLinkIt, NAME_None, EUserInterfaceActionType::Button);

		}
	}
	return MenuBuilder.MakeWidget();

}


void FControlRigEditorModule::ToggleIsDrivenByLevelSequence(UAnimSequence* AnimSequence) const
{

//todo what?
}

bool FControlRigEditorModule::IsDrivenByLevelSequence(UAnimSequence* AnimSequence) const
{
	if (AnimSequence->GetClass()->ImplementsInterface(UInterface_AssetUserData::StaticClass()))
	{
		if (IInterface_AssetUserData* AnimAssetUserData = Cast< IInterface_AssetUserData >(AnimSequence))
		{
			UAnimSequenceLevelSequenceLink* AnimLevelLink = AnimAssetUserData->GetAssetUserData< UAnimSequenceLevelSequenceLink >();
			return (AnimLevelLink != nullptr) ? true : false;
		}
	}
	return false;
}

void FControlRigEditorModule::EditWithFKControlRig(UAnimSequence* AnimSequence, USkeletalMesh* SkelMesh, USkeleton* InSkeleton)
{
	BakeToControlRig(UFKControlRig::StaticClass(),AnimSequence, SkelMesh, InSkeleton);
}


void FControlRigEditorModule::BakeToControlRig(UClass* ControlRigClass, UAnimSequence* AnimSequence, USkeletalMesh* SkelMesh, USkeleton* InSkeleton)
{
	FSlateApplication::Get().DismissAllMenus();

	UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;

	if (World)
	{
		FControlRigEditorModule::UnLinkLevelSequence(AnimSequence);

		FString SequenceName = FString::Printf(TEXT("Driving_%s"), *AnimSequence->GetName());
		FString PackagePath = AnimSequence->GetPathName();
		
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		FString UniquePackageName;
		FString UniqueAssetName;
		AssetToolsModule.Get().CreateUniqueAssetName(PackagePath / SequenceName, TEXT(""), UniquePackageName, UniqueAssetName);

		UPackage* Package = CreatePackage(*UniquePackageName);
		ULevelSequence* LevelSequence = NewObject<ULevelSequence>(Package, *UniqueAssetName, RF_Public | RF_Standalone);

		LevelSequence->Initialize(); //creates movie scene
		LevelSequence->MarkPackageDirty();
		UMovieScene* MovieScene = LevelSequence->GetMovieScene();

		FFrameRate TickResolution = MovieScene->GetTickResolution();
		float Duration = AnimSequence->GetPlayLength();
		LevelSequence->GetMovieScene()->SetPlaybackRange(0, (Duration * TickResolution).FloorToFrame().Value);

		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(LevelSequence);

		IAssetEditorInstance* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(LevelSequence, false);
		ILevelSequenceEditorToolkit* LevelSequenceEditor = static_cast<ILevelSequenceEditorToolkit*>(AssetEditor);
		TWeakPtr<ISequencer> WeakSequencer = LevelSequenceEditor ? LevelSequenceEditor->GetSequencer() : nullptr;

		if (WeakSequencer.IsValid())
		{
			ASkeletalMeshActor* MeshActor = World->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform::Identity);
			MeshActor->SetActorLabel(AnimSequence->GetName());

			FString StringName = MeshActor->GetActorLabel();
			FString AnimName = AnimSequence->GetName();
			StringName = StringName + FString(TEXT(" --> ")) + AnimName;
			MeshActor->SetActorLabel(StringName);
			if (SkelMesh)
			{
				MeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(SkelMesh);
			}
			MeshActor->RegisterAllComponents();
			TArray<TWeakObjectPtr<AActor> > ActorsToAdd;
			ActorsToAdd.Add(MeshActor);
			TArray<FGuid> ActorTracks = WeakSequencer.Pin()->AddActors(ActorsToAdd, false);
			FGuid ActorTrackGuid = ActorTracks[0];

			TArray<FGuid> SpawnableGuids = WeakSequencer.Pin()->ConvertToSpawnable(ActorTrackGuid);
			ActorTrackGuid = SpawnableGuids[0];
			UObject* SpawnedMesh = WeakSequencer.Pin()->FindSpawnedObjectOrTemplate(ActorTrackGuid);

			if (SpawnedMesh)
			{
				GCurrentLevelEditingViewportClient->GetWorld()->EditorDestroyActor(MeshActor, true);
				MeshActor = Cast<ASkeletalMeshActor>(SpawnedMesh);
				if (SkelMesh)
				{
					MeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(SkelMesh);
				}
				MeshActor->RegisterAllComponents();
			}

			//Delete binding from default animating rig
			FGuid CompGuid = WeakSequencer.Pin()->FindObjectId(*(MeshActor->GetSkeletalMeshComponent()), WeakSequencer.Pin()->GetFocusedTemplateID());
			if (CompGuid.IsValid())
			{
				if (!MovieScene->RemovePossessable(CompGuid))
				{
					MovieScene->RemoveSpawnable(CompGuid);
				}
			}

			UMovieSceneControlRigParameterTrack* Track = MovieScene->AddTrack<UMovieSceneControlRigParameterTrack>(ActorTrackGuid);
			if (Track)
			{
				USkeletalMesh* SkeletalMesh = MeshActor->GetSkeletalMeshComponent()->SkeletalMesh;
				USkeleton* Skeleton = SkeletalMesh->GetSkeleton();

				FString ObjectName = (ControlRigClass->GetName());
				ObjectName.RemoveFromEnd(TEXT("_C"));

				UControlRig* ControlRig = NewObject<UControlRig>(Track, ControlRigClass, FName(*ObjectName), RF_Transactional);
				ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());
				ControlRig->GetObjectBinding()->BindToObject(MeshActor);
				ControlRig->GetDataSourceRegistry()->RegisterDataSource(UControlRig::OwnerComponent, ControlRig->GetObjectBinding()->GetBoundObject());
				ControlRig->Initialize();
				ControlRig->Evaluate_AnyThread();

				WeakSequencer.Pin()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);

				Track->Modify();
				UMovieSceneSection* NewSection = Track->CreateControlRigSection(0, ControlRig, true);
				//mz todo need to have multiple rigs with same class
				Track->SetTrackName(FName(*ObjectName));
				Track->SetDisplayName(FText::FromString(ObjectName));
				UMovieSceneControlRigParameterSection* ParamSection = Cast<UMovieSceneControlRigParameterSection>(NewSection);
			
				FBakeToControlDelegate BakeCallback = FBakeToControlDelegate::CreateLambda([this, WeakSequencer, LevelSequence, 
					AnimSequence, MovieScene, ControlRig, ParamSection,ActorTrackGuid, Skeleton]
				(bool bKeyReduce, float KeyReduceTolerance)
				{
					if (ParamSection)
					{
						ParamSection->LoadAnimSequenceIntoThisSection(AnimSequence, MovieScene, Skeleton, bKeyReduce,
							KeyReduceTolerance);
					}
					WeakSequencer.Pin()->EmptySelection();
					WeakSequencer.Pin()->SelectSection(ParamSection);
					WeakSequencer.Pin()->ThrobSectionSelection();
					WeakSequencer.Pin()->ObjectImplicitlyAdded(ControlRig);
					FText Name = LOCTEXT("SequenceTrackFilter_ControlRigControls", "Control Rig Controls");
					WeakSequencer.Pin()->SetFilterOn(Name, true);
					WeakSequencer.Pin()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
					FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
					if (!ControlRigEditMode)
					{
						GLevelEditorModeTools().ActivateMode(FControlRigEditMode::ModeName);
						ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
					}
					if (ControlRigEditMode)
					{
						ControlRigEditMode->SetObjects(ControlRig, nullptr, WeakSequencer.Pin());
					}

					//create soft links to each other
					if (IInterface_AssetUserData* AssetUserDataInterface = Cast< IInterface_AssetUserData >(LevelSequence))
					{
						ULevelSequenceAnimSequenceLink* LevelAnimLink = NewObject<ULevelSequenceAnimSequenceLink>(LevelSequence, NAME_None, RF_Public | RF_Transactional);
						FLevelSequenceAnimSequenceLinkItem LevelAnimLinkItem;
						LevelAnimLinkItem.SkelTrackGuid = ActorTrackGuid;
						LevelAnimLinkItem.PathToAnimSequence = FSoftObjectPath(AnimSequence);
						LevelAnimLinkItem.bExportCurves = true; //mz todo to fix
						LevelAnimLinkItem.bExportTransforms = true;
						LevelAnimLinkItem.bRecordInWorldSpace = false;
						LevelAnimLink->AnimSequenceLinks.Add(LevelAnimLinkItem);
						AssetUserDataInterface->AddAssetUserData(LevelAnimLink);
					}
					if (IInterface_AssetUserData* AnimAssetUserData = Cast< IInterface_AssetUserData >(AnimSequence))
					{
						UAnimSequenceLevelSequenceLink* AnimLevelLink = AnimAssetUserData->GetAssetUserData< UAnimSequenceLevelSequenceLink >();
						if (!AnimLevelLink)
						{
							AnimLevelLink = NewObject<UAnimSequenceLevelSequenceLink>(AnimSequence, NAME_None, RF_Public | RF_Transactional);
							AnimAssetUserData->AddAssetUserData(AnimLevelLink);
						}
						AnimLevelLink->SetLevelSequence(LevelSequence);
						AnimLevelLink->SkelTrackGuid = ActorTrackGuid;
					}
				});

				FOnWindowClosed BakeClosedCallback = FOnWindowClosed::CreateLambda([](const TSharedRef<SWindow>&) { });

				BakeToControlRigDialog::GetBakeParams(BakeCallback, BakeClosedCallback);
			}
		}
	}
}

void FControlRigEditorModule::UnLinkLevelSequence(UAnimSequence* AnimSequence)
{
	if (IInterface_AssetUserData* AnimAssetUserData = Cast< IInterface_AssetUserData >(AnimSequence))
	{
		UAnimSequenceLevelSequenceLink* AnimLevelLink = AnimAssetUserData->GetAssetUserData< UAnimSequenceLevelSequenceLink >();
		if (AnimLevelLink)
		{
			ULevelSequence* LevelSequence = AnimLevelLink->ResolveLevelSequence();
			if (LevelSequence)
			{
				if (IInterface_AssetUserData* LevelSequenceUserDataInterface = Cast< IInterface_AssetUserData >(LevelSequence))
				{

					ULevelSequenceAnimSequenceLink* LevelAnimLink = LevelSequenceUserDataInterface->GetAssetUserData< ULevelSequenceAnimSequenceLink >();
					if (LevelAnimLink)
					{
						for (int32 Index = 0; Index < LevelAnimLink->AnimSequenceLinks.Num(); ++Index)
						{
							FLevelSequenceAnimSequenceLinkItem& LevelAnimLinkItem = LevelAnimLink->AnimSequenceLinks[Index];
							if (LevelAnimLinkItem.ResolveAnimSequence() == AnimSequence)
							{
								LevelAnimLink->AnimSequenceLinks.RemoveAtSwap(Index);
								break;
							}
						}
						if (LevelAnimLink->AnimSequenceLinks.Num() <= 0)
						{
							LevelSequenceUserDataInterface->RemoveUserDataOfClass(ULevelSequenceAnimSequenceLink::StaticClass());
						}
					}

				}
			}
			AnimAssetUserData->RemoveUserDataOfClass(UAnimSequenceLevelSequenceLink::StaticClass());
		}
	}
}

void FControlRigEditorModule::OpenLevelSequence(UAnimSequence* AnimSequence) 
{
	if (IInterface_AssetUserData* AnimAssetUserData = Cast< IInterface_AssetUserData >(AnimSequence))
	{
		UAnimSequenceLevelSequenceLink* AnimLevelLink = AnimAssetUserData->GetAssetUserData< UAnimSequenceLevelSequenceLink >();
		if (AnimLevelLink)
		{
			ULevelSequence* LevelSequence = AnimLevelLink->ResolveLevelSequence();
			if (LevelSequence)
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(LevelSequence);
			}
		}
	}
}

void FControlRigEditorModule::HandleAddControlRigExtenderToToolbar(FToolBarBuilder& ParentToolbarBuilder, TWeakPtr<IAnimationEditor> InAnimationEditor)
{
	ParentToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateRaw(this, &FControlRigEditorModule::GenerateAnimationMenu, InAnimationEditor),
		LOCTEXT("EditInSequencer", "Edit in Sequencer"),
		LOCTEXT("EditInSequencer_Tooltip", "Edit this Anim Sequence In Sequencer."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.ExportToFBX")
	);
}

void FControlRigEditorModule::ExtendAnimSequenceMenu()
{
	TArray<UToolMenu*> MenusToExtend;
	MenusToExtend.Add(UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.AnimSequence"));

	for (UToolMenu* Menu : MenusToExtend)
	{
		if (Menu == nullptr)
		{
			continue;
		}

		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		Section.AddDynamicEntry("GetActions", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
				if (Context)
				{

					TArray<UObject*> SelectedObjects = Context->GetSelectedObjects();
					if (SelectedObjects.Num() > 0)
					{
						InSection.AddMenuEntry(
							"OpenLevelSequence",
							LOCTEXT("OpenLevelSequence", "Open Level Sequence"),
							LOCTEXT("CreateControlRig_ToolTip", "Opens a Level Sequence if it is driving this Anim Sequence."),
							FSlateIcon(FEditorStyle::GetStyleSetName(), "GenericCurveEditor.TabIcon"),
							FUIAction(
								FExecuteAction::CreateLambda([SelectedObjects]()
									{
										for (UObject* SelectedObject : SelectedObjects)
										{
											UAnimSequence* AnimSequence = Cast<UAnimSequence>(SelectedObject);
											if (AnimSequence)
											{
												FControlRigEditorModule::OpenLevelSequence(AnimSequence);
												return; //just open up the first valid one, can't have more than one open.
											}
										}
									}),
								FCanExecuteAction::CreateLambda([SelectedObjects]()
									{
										for (UObject* SelectedObject : SelectedObjects)
										{
											UAnimSequence* AnimSequence = Cast<UAnimSequence>(SelectedObject);
											if (AnimSequence)
											{
												if (IInterface_AssetUserData* AnimAssetUserData = Cast< IInterface_AssetUserData >(AnimSequence))
												{
													UAnimSequenceLevelSequenceLink* AnimLevelLink = AnimAssetUserData->GetAssetUserData< UAnimSequenceLevelSequenceLink >();
													if (AnimLevelLink)
													{
														ULevelSequence* LevelSequence = AnimLevelLink->ResolveLevelSequence();
														if (LevelSequence)
														{
															return true;
														}
													}
												}
											}
										}
										return false;
									})

								)
						);
							
					}
				}
			}));
	}
}

void FControlRigEditorModule::HandleNewBlueprintCreated(UBlueprint* InBlueprint)
{
	// add an initial graph for us to work in
	const UControlRigGraphSchema* ControlRigGraphSchema = GetDefault<UControlRigGraphSchema>();

	UEdGraph* ControlRigGraph = FBlueprintEditorUtils::CreateNewGraph(InBlueprint, ControlRigGraphSchema->GraphName_ControlRig, UControlRigGraph::StaticClass(), UControlRigGraphSchema::StaticClass());
	ControlRigGraph->bAllowDeletion = false;
	FBlueprintEditorUtils::AddUbergraphPage(InBlueprint, ControlRigGraph);
	InBlueprint->LastEditedDocuments.AddUnique(ControlRigGraph);
	InBlueprint->PostLoad();
}

TSharedRef<IControlRigEditor> FControlRigEditorModule::CreateControlRigEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, class UControlRigBlueprint* InBlueprint)
{
	TSharedRef< FControlRigEditor > NewControlRigEditor(new FControlRigEditor());
	NewControlRigEditor->InitControlRigEditor(Mode, InitToolkitHost, InBlueprint);
	return NewControlRigEditor;
}

void FControlRigEditorModule::GetTypeActions(UControlRigBlueprint* CRB, FBlueprintActionDatabaseRegistrar& ActionRegistrar)
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the class (so if the class 
	// type disappears, then the action should go with it)
	UClass* ActionKey = CRB->GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (!ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		return;
	}

	/*
	for (const FRigVMPrototype& Prototype : FRigVMRegistry::Get().GetPrototypes())
	{
		// ignore prototype that have only one function
		if (Prototype.NumFunctions() <= 1)
		{
			continue;
		}

		FText NodeCategory = FText::FromString(Prototype.GetCategory());
		FText MenuDesc = FText::FromName(Prototype.GetName());
		FText ToolTip;

		UBlueprintNodeSpawner* NodeSpawner = UControlRigPrototypeNodeSpawner::CreateFromNotation(Prototype.GetNotation(), MenuDesc, NodeCategory, ToolTip);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	};
	*/

	// Add all rig units
	for(const FRigVMFunction& Function : FRigVMRegistry::Get().GetFunctions())
	{
		UScriptStruct* Struct = Function.Struct;
		if (!Struct->IsChildOf(FRigUnit::StaticStruct()))
		{
			continue;
		}

		// skip rig units which have a prototype
		/*
		if (Function.PrototypeIndex != INDEX_NONE)
		{
			if (FRigVMRegistry::Get().GetPrototypes()[Function.PrototypeIndex].NumFunctions() > 1)
			{
				continue;
			}
		}
		*/

		FString CategoryMetadata, DisplayNameMetadata, MenuDescSuffixMetadata;
		Struct->GetStringMetaDataHierarchical(FRigVMStruct::CategoryMetaName, &CategoryMetadata);
		Struct->GetStringMetaDataHierarchical(FRigVMStruct::DisplayNameMetaName, &DisplayNameMetadata);
		Struct->GetStringMetaDataHierarchical(FRigVMStruct::MenuDescSuffixMetaName, &MenuDescSuffixMetadata);
		if (!MenuDescSuffixMetadata.IsEmpty())
		{
			MenuDescSuffixMetadata = TEXT(" ") + MenuDescSuffixMetadata;
		}
		FText NodeCategory = FText::FromString(CategoryMetadata);
		FText MenuDesc = FText::FromString(DisplayNameMetadata + MenuDescSuffixMetadata);
		FText ToolTip = Struct->GetToolTipText();

		UBlueprintNodeSpawner* NodeSpawner = UControlRigUnitNodeSpawner::CreateFromStruct(Struct, MenuDesc, NodeCategory, ToolTip);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	};

	UBlueprintNodeSpawner* RerouteNodeSpawner = UControlRigRerouteNodeSpawner::CreateGeneric(
		LOCTEXT("RerouteSpawnerDesc", "Reroute"),
		LOCTEXT("RerouteSpawnerCategory", "Organization"), 
		LOCTEXT("RerouteSpawnerTooltip", "Adds a new reroute node to the graph"));
	ActionRegistrar.AddBlueprintAction(ActionKey, RerouteNodeSpawner);

	UBlueprintNodeSpawner* BranchNodeSpawner = UControlRigBranchNodeSpawner::CreateGeneric(
		LOCTEXT("BranchSpawnerDesc", "Branch"),
		LOCTEXT("BranchSpawnerCategory", "Execution"),
		LOCTEXT("BranchSpawnerTooltip", "Adds a new 'branch' node to the graph"));
	ActionRegistrar.AddBlueprintAction(ActionKey, BranchNodeSpawner);

	UBlueprintNodeSpawner* IfNodeSpawner = UControlRigIfNodeSpawner::CreateGeneric(
		LOCTEXT("IfSpawnerDesc", "If"),
		LOCTEXT("IfSpawnerCategory", "Execution"),
		LOCTEXT("IfSpawnerTooltip", "Adds a new 'if' node to the graph"));
	ActionRegistrar.AddBlueprintAction(ActionKey, IfNodeSpawner);

	UBlueprintNodeSpawner* SelectNodeSpawner = UControlRigSelectNodeSpawner::CreateGeneric(
		LOCTEXT("SelectSpawnerDesc", "Select"),
		LOCTEXT("SelectSpawnerCategory", "Execution"),
		LOCTEXT("SelectSpawnerTooltip", "Adds a new 'select' node to the graph"));
	ActionRegistrar.AddBlueprintAction(ActionKey, SelectNodeSpawner);

	for (TObjectIterator<UEnum> EnumIt; EnumIt; ++EnumIt)
	{
		UEnum* EnumToConsider = (*EnumIt);

		if (EnumToConsider->HasMetaData(TEXT("Hidden")))
		{
			continue;
		}

		if (EnumToConsider->IsEditorOnly())
		{
			continue;
		}

		if(EnumToConsider->IsNative())
		{
			continue;
		}

		FText NodeCategory = FText::FromString(TEXT("Enum"));
		FText MenuDesc = FText::FromString(FString::Printf(TEXT("Enum %s"), *EnumToConsider->GetName()));
		FText ToolTip = MenuDesc;

		UBlueprintNodeSpawner* NodeSpawner = UControlRigEnumNodeSpawner::CreateForEnum(EnumToConsider, MenuDesc, NodeCategory, ToolTip);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

void FControlRigEditorModule::GetInstanceActions(UControlRigBlueprint* CRB, FBlueprintActionDatabaseRegistrar& ActionRegistrar)
{
	if (UClass* GeneratedClass = CRB->GetControlRigBlueprintGeneratedClass())
	{
		if (UControlRig* CDO = Cast<UControlRig>(GeneratedClass->GetDefaultObject()))
		{
			static const FString CategoryDelimiter(TEXT("|"));
			FText NodeCategory = LOCTEXT("Variables", "Variables");

			TArray<FRigVMExternalVariable> ExternalVariables = CDO->GetExternalVariables();
			for (const FRigVMExternalVariable& ExternalVariable : ExternalVariables)
			{
				FText MenuDesc = FText::FromName(ExternalVariable.Name);
				FText ToolTip = FText::FromString(FString::Printf(TEXT("Get the value of variable %s"), *ExternalVariable.Name.ToString()));
				ActionRegistrar.AddBlueprintAction(GeneratedClass, UControlRigVariableNodeSpawner::CreateFromExternalVariable(CRB, ExternalVariable, true, MenuDesc, NodeCategory, ToolTip));

				ToolTip = FText::FromString(FString::Printf(TEXT("Set the value of variable %s"), *ExternalVariable.Name.ToString()));
				ActionRegistrar.AddBlueprintAction(GeneratedClass, UControlRigVariableNodeSpawner::CreateFromExternalVariable(CRB, ExternalVariable, false, MenuDesc, NodeCategory, ToolTip));
			}
		}

		if (URigVMFunctionLibrary* LocalFunctionLibrary = CRB->GetLocalFunctionLibrary())
		{
			TArray<URigVMLibraryNode*> Functions = LocalFunctionLibrary->GetFunctions();
			for (URigVMLibraryNode* Function : Functions)
			{
				UBlueprintNodeSpawner* NodeSpawner = UControlRigFunctionRefNodeSpawner::CreateFromFunction(Function);
				check(NodeSpawner != nullptr);
				ActionRegistrar.AddBlueprintAction(GeneratedClass, NodeSpawner);
			}
		}
	}
}

FConnectionDrawingPolicy* FControlRigEditorModule::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj)
{
	return new FControlRigConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

void FControlRigEditorModule::GetContextMenuActions(const UControlRigGraphSchema* Schema, UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	if (Menu && Context)
	{
		Schema->UEdGraphSchema::GetContextMenuActions(Menu, Context);

		if (UEdGraphPin* InGraphPin = (UEdGraphPin* )Context->Pin)
		{
			UEdGraph* Graph = InGraphPin->GetOwningNode()->GetGraph();

			// Add the watch pin / unwatch pin menu items
			{
				FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaWatches", LOCTEXT("WatchesHeader", "Watches"));
				UBlueprint* OwnerBlueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(Context->Graph);
				{
					if (FKismetDebugUtilities::IsPinBeingWatched(OwnerBlueprint, InGraphPin))
					{
						Section.AddMenuEntry(FGraphEditorCommands::Get().StopWatchingPin);
					}
					else
					{
						Section.AddMenuEntry(FGraphEditorCommands::Get().StartWatchingPin);
					}
				}
			}

			// Add alphainterp menu entries
			if(UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>((UBlueprint*)Context->Blueprint))
			{
				if (URigVMPin* ModelPin = RigBlueprint->GetModel(Graph)->FindPin(InGraphPin->GetName()))
				{
					URigVMController* Controller = RigBlueprint->GetController(ModelPin->GetGraph());

					if (ModelPin->IsArray())
					{
						FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaPinArrays", LOCTEXT("PinArrays", "Arrays"));
						Section.AddMenuEntry(
							"ClearPinArray",
							LOCTEXT("ClearPinArray", "Clear Array"),
							LOCTEXT("ClearPinArray_Tooltip", "Removes all elements of the array."),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([Controller, ModelPin]() {
								Controller->ClearArrayPin(ModelPin->GetPinPath());
							})
						));
		}
					if(ModelPin->IsArrayElement())
					{
						FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaPinArrays", LOCTEXT("PinArrays", "Arrays"));
						Section.AddMenuEntry(
							"RemoveArrayPin",
							LOCTEXT("RemoveArrayPin", "Remove Array Element"),
							LOCTEXT("RemoveArrayPin_Tooltip", "Removes the selected element from the array"),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([Controller, ModelPin]() {
								Controller->RemoveArrayPin(ModelPin->GetPinPath());
							})
						));
						Section.AddMenuEntry(
							"DuplicateArrayPin",
							LOCTEXT("DuplicateArrayPin", "Duplicate Array Element"),
							LOCTEXT("DuplicateArrayPin_Tooltip", "Duplicates the selected element"),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([Controller, ModelPin]() {
								Controller->DuplicateArrayPin(ModelPin->GetPinPath());
							})
						));
					}

					if (ModelPin->GetDirection() == ERigVMPinDirection::Input &&
							!ModelPin->IsExecuteContext())
					{
						if (ModelPin->IsBoundToVariable())
						{
							FVector2D NodePosition = FVector2D(Context->Node->NodePosX - 200.f, Context->Node->NodePosY);

							FToolMenuSection& VariablesSection = Menu->AddSection("EdGraphSchemaVariables", LOCTEXT("Variables", "Variables"));
							VariablesSection.AddMenuEntry(
								"MakeVariableNodeFromBinding",
								LOCTEXT("MakeVariableNodeFromBinding", "Make Variable Node"),
								LOCTEXT("MakeVariableNodeFromBinding_Tooltip", "Turns the variable binding on the pin to a variable node"),
								FSlateIcon(),
								FUIAction(FExecuteAction::CreateLambda([Controller, ModelPin, NodePosition]() {
									Controller->MakeVariableNodeFromBinding(ModelPin->GetPinPath(), NodePosition);
								})
							));
						}
						else
						{
							FVector2D NodePosition = FVector2D(Context->Node->NodePosX - 200.f, Context->Node->NodePosY);

							FToolMenuSection& VariablesSection = Menu->AddSection("EdGraphSchemaVariables", LOCTEXT("Variables", "Variables"));
							VariablesSection.AddMenuEntry(
								"PromotePinToVariable",
								LOCTEXT("PromotePinToVariable", "Promote Pin To Variable"),
								LOCTEXT("PromotePinToVariable_Tooltip", "Turns the variable into a variable"),
								FSlateIcon(),
								FUIAction(FExecuteAction::CreateLambda([Controller, ModelPin, NodePosition]() {

									FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();
									bool bCreateVariableNode = !KeyState.IsAltDown();

									Controller->PromotePinToVariable(ModelPin->GetPinPath(), bCreateVariableNode, NodePosition);
								})
							));
						}
					}

					if(Cast<URigVMUnitNode>(ModelPin->GetNode()) != nullptr || 
						Cast<URigVMLibraryNode>(ModelPin->GetNode()) != nullptr)
					{
						if (ModelPin->GetDirection() == ERigVMPinDirection::Input && 
							!ModelPin->IsExecuteContext() &&
							!ModelPin->IsBoundToVariable())
						{
							FToolMenuSection& VariablesSection = Menu->FindOrAddSection(TEXT("Variables"));

							TSharedRef<SControlRigVariableBinding> VariableBindingWidget =
								SNew(SControlRigVariableBinding)
								.Blueprint(RigBlueprint)
								.ModelPin(ModelPin)
								.CanRemoveBinding(false);

							VariablesSection.AddEntry(FToolMenuEntry::InitWidget("BindPinToVariableWidget", VariableBindingWidget, FText(), true));
						}

						FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaPinDefaults", LOCTEXT("PinDefaults", "Pin Defaults"));
						Section.AddMenuEntry(
							"ResetPinDefaultValue",
							LOCTEXT("ResetPinDefaultValue", "Reset Pin Value"),
							LOCTEXT("ResetPinDefaultValue_Tooltip", "Resets the pin's value to its default."),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([Controller, ModelPin]() {
								Controller->ResetPinDefaultValue(ModelPin->GetPinPath());
							})
						));
					}

					if ((ModelPin->GetCPPType() == TEXT("FVector") ||
						 ModelPin->GetCPPType() == TEXT("FQuat") ||
						 ModelPin->GetCPPType() == TEXT("FTransform")) &&
						(ModelPin->GetDirection() == ERigVMPinDirection::Input ||
						 ModelPin->GetDirection() == ERigVMPinDirection::IO) &&
						 ModelPin->GetPinForLink()->GetRootPin()->GetSourceLinks(true).Num() == 0)
					{
						FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaControlPin", LOCTEXT("ControlPin", "Direct Manipulation"));
						Section.AddMenuEntry(
							"DirectManipControlPin",
							LOCTEXT("DirectManipControlPin", "Control Pin Value"),
							LOCTEXT("DirectManipControlPin_Tooltip", "Configures the pin for direct interaction in the viewport"),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([RigBlueprint, ModelPin]() {
								RigBlueprint->AddTransientControl(ModelPin);
							})
						));
					}

					if (ModelPin->GetRootPin() == ModelPin && (
						Cast<URigVMUnitNode>(ModelPin->GetNode()) != nullptr ||
						Cast<URigVMLibraryNode>(ModelPin->GetNode()) != nullptr))
					{
						if (ModelPin->HasInjectedNodes())
						{
							FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaNodeEjectionInterp", LOCTEXT("NodeEjectionInterp", "Eject"));

							Section.AddMenuEntry(
								"EjectLastNode",
								LOCTEXT("EjectLastNode", "Eject Last Node"),
								LOCTEXT("EjectLastNode_Tooltip", "Eject the last injected node"),
								FSlateIcon(),
								FUIAction(FExecuteAction::CreateLambda([Controller, ModelPin]() {
									Controller->EjectNodeFromPin(ModelPin->GetPinPath());
								})
							));
						}

						if (ModelPin->GetCPPType() == TEXT("float") ||
							ModelPin->GetCPPType() == TEXT("FVector"))
						{
							FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaNodeInjectionInterp", LOCTEXT("NodeInjectionInterp", "Interpolate"));
							URigVMNode* InterpNode = nullptr;
							for (URigVMInjectionInfo* Injection : ModelPin->GetInjectedNodes())
							{
								FString PrototypeName;
								if (Injection->UnitNode->GetScriptStruct()->GetStringMetaDataHierarchical(TEXT("PrototypeName"), &PrototypeName))
								{
									if (PrototypeName == TEXT("AlphaInterp"))
									{
										InterpNode = Injection->UnitNode;
										break;
									}
								}
							}

							if (InterpNode == nullptr)
							{
								UScriptStruct* ScriptStruct = nullptr;

								if (ModelPin->GetCPPType() == TEXT("float"))
								{
									ScriptStruct = FRigUnit_AlphaInterp::StaticStruct();
								}
								else if (ModelPin->GetCPPType() == TEXT("FVector"))
								{
									ScriptStruct = FRigUnit_AlphaInterpVector::StaticStruct();
								}
								else
								{
									checkNoEntry();
								}

								Section.AddMenuEntry(
									"AddAlphaInterp",
									LOCTEXT("AddAlphaInterp", "Add Interpolate"),
									LOCTEXT("AddAlphaInterp_Tooltip", "Injects an interpolate node"),
									FSlateIcon(),
									FUIAction(FExecuteAction::CreateLambda([Controller, InGraphPin, ModelPin, ScriptStruct]() {
										URigVMInjectionInfo* Injection = Controller->AddInjectedNode(ModelPin->GetPinPath(), ModelPin->GetDirection() != ERigVMPinDirection::Output, ScriptStruct, TEXT("Execute"), TEXT("Value"), TEXT("Result"));
										if (Injection)
										{
											TArray<FName> NodeNames;
											NodeNames.Add(Injection->UnitNode->GetFName());
											Controller->SetNodeSelection(NodeNames);
										}
									})
								));
							}
							else
							{
								Section.AddMenuEntry(
									"EditAlphaInterp",
									LOCTEXT("EditAlphaInterp", "Edit Interpolate"),
									LOCTEXT("EditAlphaInterp_Tooltip", "Edit the interpolate node"),
									FSlateIcon(),
									FUIAction(FExecuteAction::CreateLambda([RigBlueprint, InterpNode]() {
									TArray<FName> NodeNames;
									NodeNames.Add(InterpNode->GetFName());
									RigBlueprint->GetController(InterpNode->GetGraph())->SetNodeSelection(NodeNames);
								})
									));
								Section.AddMenuEntry(
									"RemoveAlphaInterp",
									LOCTEXT("RemoveAlphaInterp", "Remove Interpolate"),
									LOCTEXT("RemoveAlphaInterp_Tooltip", "Removes the interpolate node"),
									FSlateIcon(),
									FUIAction(FExecuteAction::CreateLambda([Controller, InGraphPin, ModelPin, InterpNode]() {
										Controller->RemoveNodeByName(InterpNode->GetFName());
									})
								));
							}
						}

						if (ModelPin->GetCPPType() == TEXT("FVector") ||
							ModelPin->GetCPPType() == TEXT("FQuat") ||
							ModelPin->GetCPPType() == TEXT("FTransform"))
						{
							FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaNodeInjectionVisualDebug", LOCTEXT("NodeInjectionVisualDebug", "Visual Debug"));

							URigVMNode* VisualDebugNode = nullptr;
							for (URigVMInjectionInfo* Injection : ModelPin->GetInjectedNodes())
							{
								FString PrototypeName;
								if (Injection->UnitNode->GetScriptStruct()->GetStringMetaDataHierarchical(TEXT("PrototypeName"), &PrototypeName))
								{
									if (PrototypeName == TEXT("VisualDebug"))
									{
										VisualDebugNode = Injection->UnitNode;
										break;
									}
								}
							}

							if (VisualDebugNode == nullptr)
							{
								UScriptStruct* ScriptStruct = nullptr;

								if (ModelPin->GetCPPType() == TEXT("FVector"))
								{
									ScriptStruct = FRigUnit_VisualDebugVectorItemSpace::StaticStruct();
								}
								else if (ModelPin->GetCPPType() == TEXT("FQuat"))
								{
									ScriptStruct = FRigUnit_VisualDebugQuatItemSpace::StaticStruct();
								}
								else if (ModelPin->GetCPPType() == TEXT("FTransform"))
								{
									ScriptStruct = FRigUnit_VisualDebugTransformItemSpace::StaticStruct();
								}
								else
								{
									checkNoEntry();
								}

								Section.AddMenuEntry(
									"AddVisualDebug",
									LOCTEXT("AddVisualDebug", "Add Visual Debug"),
									LOCTEXT("AddVisualDebug_Tooltip", "Injects a visual debugging node"),
									FSlateIcon(),
									FUIAction(FExecuteAction::CreateLambda([RigBlueprint, Controller, InGraphPin, ModelPin, ScriptStruct]() {
										URigVMInjectionInfo* Injection = Controller->AddInjectedNode(ModelPin->GetPinPath(), ModelPin->GetDirection() != ERigVMPinDirection::Output, ScriptStruct, TEXT("Execute"), TEXT("Value"), TEXT("Value"));
										if (Injection)
										{
											TArray<FName> NodeNames;
											NodeNames.Add(Injection->UnitNode->GetFName());
											Controller->SetNodeSelection(NodeNames);

											if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(ModelPin->GetNode()))
											{
												if (TSharedPtr<FStructOnScope> DefaultStructScope = UnitNode->ConstructStructInstance())
												{
													FRigUnit* DefaultStruct = (FRigUnit*)DefaultStructScope->GetStructMemory();

													FString PinPath = ModelPin->GetPinPath();
													FString Left, Right;

													FRigElementKey SpaceKey;
													if (URigVMPin::SplitPinPathAtStart(PinPath, Left, Right))
													{
														SpaceKey = DefaultStruct->DetermineSpaceForPin(Right, &RigBlueprint->HierarchyContainer);
													}

													if (SpaceKey.IsValid())
													{
														if (URigVMPin* SpacePin = Injection->UnitNode->FindPin(TEXT("Space")))
														{
															if(URigVMPin* SpaceTypePin = SpacePin->FindSubPin(TEXT("Type")))
															{
																FString SpaceTypeStr = StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)SpaceKey.Type).ToString();
																Controller->SetPinDefaultValue(SpaceTypePin->GetPinPath(), SpaceTypeStr);
															}
															if(URigVMPin* SpaceNamePin = SpacePin->FindSubPin(TEXT("Name")))
															{
																Controller->SetPinDefaultValue(SpaceNamePin->GetPinPath(), SpaceKey.Name.ToString());
															}
														}
													}
												}
											}
										}
									})
								));
							}
							else
							{
								Section.AddMenuEntry(
									"EditVisualDebug",
									LOCTEXT("EditVisualDebug", "Edit Visual Debug"),
									LOCTEXT("EditVisualDebug_Tooltip", "Edit the visual debugging node"),
									FSlateIcon(),
									FUIAction(FExecuteAction::CreateLambda([Controller, VisualDebugNode]() {
										TArray<FName> NodeNames;
										NodeNames.Add(VisualDebugNode->GetFName());
										Controller->SetNodeSelection(NodeNames);
									})
								));
								Section.AddMenuEntry(
									"ToggleVisualDebug",
									LOCTEXT("ToggleVisualDebug", "Toggle Visual Debug"),
									LOCTEXT("ToggleVisualDebug_Tooltip", "Toggle the visibility the visual debugging"),
									FSlateIcon(),
									FUIAction(FExecuteAction::CreateLambda([Controller, VisualDebugNode]() {
										URigVMPin* EnabledPin = VisualDebugNode->FindPin(TEXT("bEnabled"));
										check(EnabledPin);
										Controller->SetPinDefaultValue(EnabledPin->GetPinPath(), EnabledPin->GetDefaultValue() == TEXT("True") ? TEXT("False") : TEXT("True"), false);
									})
								));
								Section.AddMenuEntry(
									"RemoveVisualDebug",
									LOCTEXT("RemoveVisualDebug", "Remove Visual Debug"),
									LOCTEXT("RemoveVisualDebug_Tooltip", "Removes the visual debugging node"),
									FSlateIcon(),
										FUIAction(FExecuteAction::CreateLambda([Controller, InGraphPin, ModelPin, VisualDebugNode]() {
										Controller->RemoveNodeByName(VisualDebugNode->GetFName());
									})
								));
							}
						}
					}
				}
			}
		}
		else if(Context->Node) // right clicked on the node
		{
			if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>((UBlueprint*)Context->Blueprint))
			{
				URigVMGraph* Model = RigBlueprint->GetModel(Context->Node->GetGraph());
				URigVMController* Controller = RigBlueprint->GetController(Model);

				TArray<FRigElementKey> RigElementsToSelect;
				TMap<const URigVMPin*, FRigElementKey> PinToKey;
				TArray<FName> SelectedNodeNames = Model->GetSelectNodes();
				SelectedNodeNames.AddUnique(Context->Node->GetFName());

				for(const FName& SelectedNodeName : SelectedNodeNames)
				{
					if (URigVMNode* ModelNode = Model->FindNodeByName(SelectedNodeName))
					{
						TSharedPtr<FStructOnScope> StructOnScope;
						FRigHierarchyContainer TemporaryHierarchy = RigBlueprint->HierarchyContainer;
						FRigUnit* StructMemory = nullptr;
						UScriptStruct* ScriptStruct = nullptr;
						if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(ModelNode))
						{
							ScriptStruct = UnitNode->GetScriptStruct();
							StructOnScope = UnitNode->ConstructStructInstance(false /* default */);
							StructMemory = (FRigUnit*)StructOnScope->GetStructMemory();

							FRigUnitContext RigUnitContext;
							RigUnitContext.Hierarchy = &TemporaryHierarchy;
							RigUnitContext.State = EControlRigState::Update;
							StructMemory->Execute(RigUnitContext);
						}

						for (const URigVMPin* Pin : ModelNode->GetAllPinsRecursively())
						{
							if (Pin->GetCPPType() == TEXT("FName"))
							{
								FRigElementKey Key;
								if (Pin->GetCustomWidgetName() == TEXT("BoneName"))
								{
									Key = FRigElementKey(*Pin->GetDefaultValue(), ERigElementType::Bone);
								}
								else if (Pin->GetCustomWidgetName() == TEXT("ControlName"))
								{
									Key = FRigElementKey(*Pin->GetDefaultValue(), ERigElementType::Control);
								}
								else if (Pin->GetCustomWidgetName() == TEXT("SpaceName"))
								{
									Key = FRigElementKey(*Pin->GetDefaultValue(), ERigElementType::Space);
								}
								else if (Pin->GetCustomWidgetName() == TEXT("CurveName"))
								{
									Key = FRigElementKey(*Pin->GetDefaultValue(), ERigElementType::Curve);
								}
								else
								{
									continue;
								}

								RigElementsToSelect.AddUnique(Key);
								PinToKey.Add(Pin, Key);
							}
							else if (Pin->GetCPPTypeObject() == FRigElementKey::StaticStruct())
							{
								if (StructMemory == nullptr)
								{
									FString DefaultValue = Pin->GetDefaultValue();
									if (!DefaultValue.IsEmpty())
									{
										FRigElementKey Key;
										FRigElementKey::StaticStruct()->ImportText(*DefaultValue, &Key, nullptr, EPropertyPortFlags::PPF_None, nullptr, FRigElementKey::StaticStruct()->GetName(), true);
										if (Key.IsValid())
										{
											RigElementsToSelect.AddUnique(Key);
											if (URigVMPin* NamePin = Pin->FindSubPin(TEXT("Name")))
											{
												PinToKey.Add(NamePin, Key);
											}
										}
									}
								}
								else
								{
									check(ScriptStruct);
									if (const FProperty* Property = ScriptStruct->FindPropertyByName(Pin->GetFName()))
									{
										const FRigElementKey& Key = *Property->ContainerPtrToValuePtr<FRigElementKey>(StructMemory);

										if (Key.IsValid())
										{
											RigElementsToSelect.AddUnique(Key);

											if (URigVMPin* NamePin = Pin->FindSubPin(TEXT("Name")))
											{
												PinToKey.Add(NamePin, Key);
											}
										}
									}
								}
							}
							else if (Pin->GetCPPTypeObject() == FRigElementKeyCollection::StaticStruct() && Pin->GetDirection() == ERigVMPinDirection::Output)
							{
								if (StructMemory == nullptr)
								{
									// not supported for now
								}
								else
								{
									check(ScriptStruct);
									if (const FProperty* Property = ScriptStruct->FindPropertyByName(Pin->GetFName()))
									{
										const FRigElementKeyCollection& Collection = *Property->ContainerPtrToValuePtr<FRigElementKeyCollection>(StructMemory);

										if (Collection.Num() > 0)
										{
											RigElementsToSelect.Reset();
											for (const FRigElementKey& Item : Collection)
											{
												RigElementsToSelect.AddUnique(Item);
											}
											break;
										}
									}
								}
							}
						}
					}
				}

				if (RigElementsToSelect.Num() > 0)
				{
					FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaHierarchy", LOCTEXT("HierarchyHeader", "Hierarchy"));
					Section.AddMenuEntry(
						"SelectRigElements",
						LOCTEXT("SelectRigElements", "Select Rig Elements"),
						LOCTEXT("SelectRigElements_Tooltip", "Selects the bone, controls or spaces associated with this node."),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([RigBlueprint, RigElementsToSelect]() {

							RigBlueprint->HierarchyContainer.ClearSelection();
							for(const FRigElementKey& RigElementToSelect : RigElementsToSelect)
							{
								RigBlueprint->HierarchyContainer.Select(RigElementToSelect, true);
							}

						})
					));
				}

				if (RigElementsToSelect.Num() > 0)
				{
					FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaHierarchy", LOCTEXT("ToolsHeader", "Tools"));
					Section.AddMenuEntry(
						"SearchAndReplaceNames",
						LOCTEXT("SearchAndReplaceNames", "Search & Replace / Mirror"),
						LOCTEXT("SearchAndReplaceNames_Tooltip", "Searches within all names and replaces with a different text."),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([RigBlueprint, Controller, PinToKey]() {

							FRigMirrorSettings Settings;
							TSharedPtr<FStructOnScope> StructToDisplay = MakeShareable(new FStructOnScope(FRigMirrorSettings::StaticStruct(), (uint8*)&Settings));

							TSharedRef<SKismetInspector> KismetInspector = SNew(SKismetInspector);
							KismetInspector->ShowSingleStruct(StructToDisplay);

							SGenericDialogWidget::OpenDialog(LOCTEXT("ControlRigHierarchyMirror", "Mirror Graph"), KismetInspector, SGenericDialogWidget::FArguments(), true);

							Controller->OpenUndoBracket(TEXT("Mirroring Graph"));
							int32 ReplacedNames = 0;
							TArray<FString> UnchangedItems;

							for (const TPair<const URigVMPin*, FRigElementKey>& Pair : PinToKey)
							{
								const URigVMPin* Pin = Pair.Key;
								FRigElementKey Key = Pair.Value;

								if (Key.Name.IsNone())
								{
									continue;
								}

								FString OldNameStr = Key.Name.ToString();
								FString NewNameStr = OldNameStr.Replace(*Settings.OldName, *Settings.NewName, ESearchCase::CaseSensitive);
								if(NewNameStr != OldNameStr)
								{
									Key.Name = *NewNameStr;
									if(RigBlueprint->HierarchyContainer.GetIndex(Key) != INDEX_NONE)
									{
										Controller->SetPinDefaultValue(Pin->GetPinPath(), NewNameStr, false);
										ReplacedNames++;
									}
									else
									{
										// save the names of the items that we skipped during this search & replace
										UnchangedItems.AddUnique(OldNameStr);
									} 
								}
							}

							if (UnchangedItems.Num() > 0)
							{
								FString ListOfUnchangedItems;
								for (int Index = 0; Index < UnchangedItems.Num(); Index++)
								{
									// construct the string "item1, item2, item3"
									ListOfUnchangedItems += UnchangedItems[Index];
									if (Index != UnchangedItems.Num() - 1)
									{
										ListOfUnchangedItems += TEXT(", ");
									}
								}
								
								// inform the user that some items were skipped due to invalid new names
								Controller->ReportAndNotifyError(FString::Printf(TEXT("Invalid Names after Search & Replace, action skipped for %s"), *ListOfUnchangedItems));
							}

							if (ReplacedNames > 0)
							{ 
								Controller->CloseUndoBracket();
							}
							else
							{
								Controller->CancelUndoBracket();
							}
						})
					));
				}

				if (const UControlRigGraphNode* RigNode = Cast<const UControlRigGraphNode>(Context->Node))
				{
					if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(RigNode->GetModelNode()))
					{
						FToolMenuSection& SettingsSection = Menu->AddSection("EdGraphSchemaSettings", LOCTEXT("SettingsHeader", "Settings"));
						SettingsSection.AddMenuEntry(
							"Save Default Expansion State",
							LOCTEXT("SaveDefaultExpansionState", "Save Default Expansion State"),
							LOCTEXT("SaveDefaultExpansionState_Tooltip", "Saves the expansion state of all pins of the node as the default."),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([UnitNode]() {

#if WITH_EDITORONLY_DATA

								FScopedTransaction Transaction(LOCTEXT("RigUnitDefaultExpansionStateChanged", "Changed Rig Unit Default Expansion State"));
								UControlRigSettings::Get()->Modify();

								FControlRigSettingsPerPinBool& ExpansionMap = UControlRigSettings::Get()->RigUnitPinExpansion.FindOrAdd(UnitNode->GetScriptStruct()->GetName());
								ExpansionMap.Values.Empty();

								TArray<URigVMPin*> Pins = UnitNode->GetAllPinsRecursively();
								for (URigVMPin* Pin : Pins)
								{
									if (Pin->GetSubPins().Num() == 0)
									{
										continue;
									}

									FString PinPath = Pin->GetPinPath();
									FString NodeName, RemainingPath;
									URigVMPin::SplitPinPathAtStart(PinPath, NodeName, RemainingPath);
									ExpansionMap.Values.FindOrAdd(RemainingPath) = Pin->IsExpanded();
								}
#endif
							})
						));
					}

					if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(RigNode->GetModelNode()))
					{
						FToolMenuSection& VariablesSection = Menu->AddSection("EdGraphSchemaVariables", LOCTEXT("VariablesSettingsHeader", "Variables"));
						VariablesSection.AddMenuEntry(
							"MakePindingsFromVariableNode",
							LOCTEXT("MakeBindingsFromVariableNode", "Make Bindings From Node"),
							LOCTEXT("MakeBindingsFromVariableNode_Tooltip", "Turns the variable node into one ore more variable bindings on the pin(s)"),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([RigBlueprint, Controller, VariableNode]() {
								Controller->MakeBindingsFromVariableNode(VariableNode->GetFName());
							})
						));
					}

					FToolMenuSection& OrganizationSection = Menu->AddSection("EdGraphSchemaOrganization", LOCTEXT("OrganizationHeader", "Organization"));
					OrganizationSection.AddMenuEntry(
						"Collapse Nodes",
						LOCTEXT("CollapseNodes", "Collapse Nodes"),
						LOCTEXT("CollapseNodes_Tooltip", "Turns the selected nodes into a single Collapse node"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([Model, Controller]() {
							TArray<FName> Nodes = Model->GetSelectNodes();
							Controller->CollapseNodes(Nodes);
						})
					));

					if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(RigNode->GetModelNode()))
					{
						OrganizationSection.AddMenuEntry(
							"Promote To Function",
							LOCTEXT("PromoteToFunction", "Promote To Function"),
							LOCTEXT("PromoteToFunction_Tooltip", "Turns the Collapse Node into a Function"),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([Controller, CollapseNode]() {
								Controller->PromoteCollapseNodeToFunctionReferenceNode(CollapseNode->GetFName());
							})
						));
					}

					OrganizationSection.AddSubMenu("Alignment", LOCTEXT("AlignmentHeader", "Alignment"), FText(), FNewToolMenuDelegate::CreateLambda([](UToolMenu* AlignmentMenu)
					{
						{
							FToolMenuSection& InSection = AlignmentMenu->AddSection("EdGraphSchemaAlignment", LOCTEXT("AlignHeader", "Align"));
							InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesTop);
							InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesMiddle);
							InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesBottom);
							InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesLeft);
							InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesCenter);
							InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesRight);
							InSection.AddMenuEntry(FGraphEditorCommands::Get().StraightenConnections);
						}

						{
							FToolMenuSection& InSection = AlignmentMenu->AddSection("EdGraphSchemaDistribution", LOCTEXT("DistributionHeader", "Distribution"));
							InSection.AddMenuEntry(FGraphEditorCommands::Get().DistributeNodesHorizontally);
							InSection.AddMenuEntry(FGraphEditorCommands::Get().DistributeNodesVertically);
						}
					}));

					if (URigVMFunctionReferenceNode* FunctionRefNode = Cast<URigVMFunctionReferenceNode>(RigNode->GetModelNode()))
					{
						OrganizationSection.AddMenuEntry(
							"Promote To Collapse Node",
							LOCTEXT("PromoteToCollapseNode", "Promote To Collapse Node"),
							LOCTEXT("PromoteToCollapseNode_Tooltip", "Turns the Function Ref Node into a Collapse Node"),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([Controller, FunctionRefNode]() {
								Controller->PromoteFunctionReferenceNodeToCollapseNode(FunctionRefNode->GetFName());
								})
							));
					}

					if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(RigNode->GetModelNode()))
					{
						OrganizationSection.AddMenuEntry(
							"Expand Node",
							LOCTEXT("ExpandNode", "Expand Node"),
							LOCTEXT("ExpandNode_Tooltip", "Expands the contents of the node into this graph"),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([Controller, LibraryNode]() {
								Controller->OpenUndoBracket(TEXT("Expand node"));
								TArray<URigVMNode*> ExpandedNodes = Controller->ExpandLibraryNode(LibraryNode->GetFName());
								if (ExpandedNodes.Num() > 0)
								{
									TArray<FName> ExpandedNodeNames;
									for (URigVMNode* ExpandedNode : ExpandedNodes)
									{
										ExpandedNodeNames.Add(ExpandedNode->GetFName());
									}
									Controller->SetNodeSelection(ExpandedNodeNames);
								}
								Controller->CloseUndoBracket();
							})
						));
					}
				}
			}
		}
	}
}

IMPLEMENT_MODULE(FControlRigEditorModule, ControlRigEditor)

#undef LOCTEXT_NAMESPACE
