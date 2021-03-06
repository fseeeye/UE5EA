// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSkeletalMeshEditorToolbox.h"

#include "ISkeletalMeshEditor.h"

#include "EdMode.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Tools/UEdMode.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SUniformWrapPanel.h"


SSkeletalMeshEditorToolbox::~SSkeletalMeshEditorToolbox()
{
}


void SSkeletalMeshEditorToolbox::Construct(
	const FArguments& InArgs, 
	const TSharedRef<ISkeletalMeshEditor>& InOwningEditor
	)
{
	SkeletalMeshEditor = InOwningEditor;

	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
		.Padding(0.0f)
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign( HAlign_Left )
			[
				SAssignNew( ModeToolBarContainer, SBorder )
				.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
				.Padding( FMargin(4, 0, 0, 0) )
			]

			+ SVerticalBox::Slot()
			.FillHeight( 1.0f )
			[
				SNew( SVerticalBox )

				+SVerticalBox::Slot()
				.Padding(0.0, 8.0, 0.0, 0.0)
				.AutoHeight()
				[
					SAssignNew(ModeToolHeader, SBorder)
					.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
				]

				+ SVerticalBox::Slot()
				.FillHeight(1)
				[
					SAssignNew(InlineContentHolder, SBorder)
					.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
					.Visibility( this, &SSkeletalMeshEditorToolbox::GetInlineContentHolderVisibility )
				]
			]
		]
	];
}


void SSkeletalMeshEditorToolbox::AttachToolkit(const TSharedRef<IToolkit>& InToolkit)
{
	UpdateInlineContent(InToolkit, InToolkit->GetInlineContent());
}


void SSkeletalMeshEditorToolbox::DetachToolkit(const TSharedRef<IToolkit>& InToolkit)
{
	UpdateInlineContent(nullptr, SNullWidget::NullWidget);
}


void SSkeletalMeshEditorToolbox::SetOwningTab(TSharedRef<SDockTab>& InOwningTab)
{
	OwningTab = InOwningTab;
}


void SSkeletalMeshEditorToolbox::UpdateInlineContent(const TSharedPtr<IToolkit>& Toolkit, TSharedPtr<SWidget> InlineContent)
{
	static const FName SkeletalMeshEditorStatusBarName = "SkeletalMeshEditor.StatusBar";

	// The display name that the owning tab should have as its label
	FText TabName;

	// The icon that should be displayed in the parent tab
	const FSlateBrush* TabIcon = nullptr;

	if (StatusBarMessageHandle.IsValid())
	{
		GEditor->GetEditorSubsystem<UStatusBarSubsystem>()->PopStatusBarMessage(SkeletalMeshEditorStatusBarName, StatusBarMessageHandle);
		StatusBarMessageHandle.Reset();
	}

	if (Toolkit.IsValid())
	{
		TabName = Toolkit->GetEditorModeDisplayName();
		TabIcon = Toolkit->GetEditorModeIcon().GetSmallIcon();

		TWeakPtr<FModeToolkit> ModeToolkit = StaticCastSharedPtr<FModeToolkit>(Toolkit);

		if (ModeToolkit.IsValid())
		{
			TSharedRef<FModeToolkit> ModeToolkitPinned = ModeToolkit.Pin().ToSharedRef();

			UpdatePalette(ModeToolkitPinned);

			// Show the name of the active tool in the statusbar.
			// FIXME: We should also be showing Ctrl/Shift/Alt LMB/RMB shortcuts.
			StatusBarMessageHandle = GEditor->GetEditorSubsystem<UStatusBarSubsystem>()->PushStatusBarMessage(
					SkeletalMeshEditorStatusBarName,
					TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(ModeToolkitPinned, &FModeToolkit::GetActiveToolDisplayName)));
		}
	}
	else
	{
		TabName = NSLOCTEXT("SkeletalMeshEditor", "ToolboxTab", "Toolbox");
		TabIcon = FEditorStyle::Get().GetBrush("LevelEditor.Tabs.Modes");
	}

	if (InlineContent.IsValid() && InlineContentHolder.IsValid())
	{
		InlineContentHolder->SetContent(InlineContent.ToSharedRef());
	}

	TSharedPtr<SDockTab> OwningTabPinned = OwningTab.Pin();
	if (OwningTabPinned.IsValid())
	{
		OwningTabPinned->SetLabel(TabName);
		OwningTabPinned->SetTabIcon(TabIcon);
	}
}


void SSkeletalMeshEditorToolbox::UpdatePalette(const TSharedRef<FModeToolkit>& InModeToolkit)
{
	TSharedRef<SUniformWrapPanel> PaletteTabBox = SNew(SUniformWrapPanel)
	                                                  .SlotPadding(FMargin(1.f, 2.f))
	                                                  .HAlign(HAlign_Center);

	// Only show if there's more than one entry.
	PaletteTabBox->SetVisibility(TAttribute<EVisibility>::Create(
		TAttribute<EVisibility>::FGetter::CreateLambda([PaletteTabBox]() -> EVisibility { 
			return PaletteTabBox->GetChildren()->Num() > 1 ? EVisibility::Visible : EVisibility::Collapsed; 
		})));

	// Also build the toolkit here
	TArray<FName> PaletteNames;
	InModeToolkit->GetToolPaletteNames(PaletteNames);

	TSharedPtr<FUICommandList> CommandList;
	CommandList = InModeToolkit->GetToolkitCommands();

	TSharedRef< SWidgetSwitcher > PaletteSwitcher = SNew(SWidgetSwitcher)
	.WidgetIndex_Lambda( [PaletteNames, InModeToolkit] () -> int32 { 
		int32 FoundIndex;
		if (PaletteNames.Find(InModeToolkit->GetCurrentPalette(), FoundIndex))
		{
			return FoundIndex;	
		}
		return 0;
	} );
			
	for(auto Palette : PaletteNames)
	{
		FName ToolbarCustomizationName = InModeToolkit->GetEditorMode() ? InModeToolkit->GetEditorMode()->GetModeInfo().ToolbarCustomizationName : InModeToolkit->GetScriptableEditorMode()->GetModeInfo().ToolbarCustomizationName;
		FUniformToolBarBuilder ModeToolbarBuilder(CommandList, FMultiBoxCustomization(ToolbarCustomizationName));
		ModeToolbarBuilder.SetStyle(&FEditorStyle::Get(), "PaletteToolBar");

		InModeToolkit->BuildToolPalette(Palette, ModeToolbarBuilder);

		TSharedRef<SWidget> PaletteWidget = ModeToolbarBuilder.MakeWidget();

		PaletteTabBox->AddSlot()
		[
			SNew(SCheckBox)
			.Padding(FMargin(8.f, 4.f, 8.f, 5.f))
			.Style( FEditorStyle::Get(),  "PaletteToolBar.Tab" )
			.OnCheckStateChanged_Lambda([/*PaletteSwitcher, PaletteWidget, */InModeToolkit, Palette](const ECheckBoxState) {
					InModeToolkit->SetCurrentPalette(Palette);
				} 
			)
			// .IsChecked_Lambda( [PaletteSwitcher, PaletteWidget] () -> ECheckBoxState { return PaletteSwitcher->GetActiveWidget() == PaletteWidget ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.IsChecked_Lambda([InModeToolkit, Palette]() -> ECheckBoxState {
				if (InModeToolkit->GetCurrentPalette() == Palette)
				{
					return ECheckBoxState::Checked;
				}
				return ECheckBoxState::Unchecked;
			})
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "NormalText")
				.Text(InModeToolkit->GetToolPaletteDisplayName(Palette))
				.Justification(ETextJustify::Center)
			]
		];


		PaletteSwitcher->AddSlot()
		[
			PaletteWidget
		]; 
	}


	ModeToolHeader->SetContent(
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.Padding(8.f, 0.f, 0.f, 8.f)
		.AutoHeight()
		[
			PaletteTabBox
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			PaletteSwitcher
		]
	);
}


EVisibility SSkeletalMeshEditorToolbox::GetInlineContentHolderVisibility() const
{
	return InlineContentHolder->GetContent() == SNullWidget::NullWidget ? EVisibility::Collapsed : EVisibility::Visible;
}
