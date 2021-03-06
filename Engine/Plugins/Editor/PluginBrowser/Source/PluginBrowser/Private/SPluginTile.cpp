// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPluginTile.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "PluginDescriptor.h"
#include "Interfaces/IPluginManager.h"
#include "SPluginBrowser.h"
#include "PluginStyle.h"
#include "GameProjectGenerationModule.h"
#include "IDetailsView.h"
#include "Widgets/Input/SHyperlink.h"
#include "PluginMetadataObject.h"
#include "Interfaces/IProjectManager.h"
#include "PluginBrowserModule.h"
#include "PropertyEditorModule.h"
#include "IUATHelperModule.h"
#include "DesktopPlatformModule.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "PluginListTile"


void SPluginTile::Construct( const FArguments& Args, const TSharedRef<SPluginTileList> Owner, TSharedRef<IPlugin> InPlugin )
{
	OwnerWeak = Owner;
	Plugin = InPlugin;

	RecreateWidgets();
}

FText SPluginTile::GetPluginNameText() const
{
	return FText::FromString(Plugin->GetFriendlyName());
}

void SPluginTile::RecreateWidgets()
{
	const float PaddingAmount = FPluginStyle::Get()->GetFloat( "PluginTile.Padding" );
	const float ThumbnailImageSize = FPluginStyle::Get()->GetFloat( "PluginTile.ThumbnailImageSize" );

	// @todo plugedit: Also display whether plugin is editor-only, runtime-only, developer or a combination?
	//		-> Maybe a filter for this too?  (show only editor plugins, etc.)
	// @todo plugedit: Indicate whether plugin has content?  Filter to show only content plugins, and vice-versa?

	// @todo plugedit: Maybe we should do the FileExists check ONCE at plugin load time and not at query time

	const FPluginDescriptor& PluginDescriptor = Plugin->GetDescriptor();

	// Plugin thumbnail image
	FString Icon128FilePath = Plugin->GetBaseDir() / TEXT("Resources/Icon128.png");
	if(!FPlatformFileManager::Get().GetPlatformFile().FileExists(*Icon128FilePath))
	{
		Icon128FilePath = IPluginManager::Get().FindPlugin(TEXT("PluginBrowser"))->GetBaseDir() / TEXT("Resources/DefaultIcon128.png");
	}

	const FName BrushName( *Icon128FilePath );
	const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
	if ((Size.X > 0) && (Size.Y > 0))
	{
		PluginIconDynamicImageBrush = MakeShareable(new FSlateDynamicImageBrush(BrushName, FVector2D(Size.X, Size.Y)));
	}

	// create support link
	TSharedPtr<SWidget> SupportWidget;
	{
		if (PluginDescriptor.SupportURL.IsEmpty())
		{
			SupportWidget = SNullWidget::NullWidget;
		}
		else
		{
			FString SupportURL = PluginDescriptor.SupportURL;
			SupportWidget = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FEditorStyle::GetBrush("Icons.Contact"))
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SHyperlink)
					.Text(LOCTEXT("SupportLink", "Support"))
					.ToolTipText(FText::Format(LOCTEXT("NavigateToSupportURL", "Open the plug-in's online support ({0})"), FText::FromString(SupportURL)))
					.OnNavigate_Lambda([=]() { FPlatformProcess::LaunchURL(*SupportURL, nullptr, nullptr); })
				];
		}
	}

	// create documentation link
	TSharedPtr<SWidget> DocumentationWidget;
	{
		if (PluginDescriptor.DocsURL.IsEmpty())
		{
			DocumentationWidget = SNullWidget::NullWidget;
		}
		else
		{
			FString DocsURL = PluginDescriptor.DocsURL;
			DocumentationWidget = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FEditorStyle::GetBrush("MessageLog.Docs"))
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SHyperlink)
						.Text(LOCTEXT("DocumentationLink", "Documentation"))
						.ToolTipText(FText::Format(LOCTEXT("NavigateToDocumentation", "Open the plug-in's online documentation ({0})"), FText::FromString(DocsURL)))
						.OnNavigate_Lambda([=]() { FPlatformProcess::LaunchURL(*DocsURL, nullptr, nullptr); })
				];
		}
	}

	// create vendor link
	TSharedPtr<SWidget> CreatedByWidget;
	{
		if (PluginDescriptor.CreatedBy.IsEmpty())
		{
			CreatedByWidget = SNullWidget::NullWidget;
		}
		else if (PluginDescriptor.CreatedByURL.IsEmpty())
		{
			CreatedByWidget = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderDeveloper"))
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(PluginDescriptor.CreatedBy))
				];
		}
		else
		{
			FString CreatedByURL = PluginDescriptor.CreatedByURL;
			CreatedByWidget = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FEditorStyle::GetBrush("MessageLog.Url"))
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				[				
					SNew(SHyperlink)
						.Text(FText::FromString(PluginDescriptor.CreatedBy))
						.ToolTipText(FText::Format(LOCTEXT("NavigateToCreatedByURL", "Visit the vendor's web site ({0})"), FText::FromString(CreatedByURL)))
						.OnNavigate_Lambda([=]() { FPlatformProcess::LaunchURL(*CreatedByURL, nullptr, nullptr); })
				];
		}
	}

	TSharedRef<SWidget> RestrictedPluginWidget = SNullWidget::NullWidget;
	if (FPaths::IsRestrictedPath(Plugin->GetDescriptorFileName()))
	{
		RestrictedPluginWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				.Padding(2.0f, 0.0f, 8.0f, 1.0f)
				[
					SNew(STextBlock)
						.TextStyle(FPluginStyle::Get(), "PluginTile.BetaText")
						.Text(LOCTEXT("PluginRestrictedText", "[Restricted]"))
						.ToolTipText(FText::AsCultureInvariant(Plugin->GetDescriptorFileName()))
				];
	}

	ChildSlot
	[
		SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.Padding(PaddingAmount)
			[
				SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(PaddingAmount)
					[
						SNew(SHorizontalBox)

						// Thumbnail image
						+ SHorizontalBox::Slot()
							.Padding(PaddingAmount)
							.AutoWidth()
							[
								SNew(SBox)
									.VAlign(VAlign_Top)
									.WidthOverride(ThumbnailImageSize)
									.HeightOverride(ThumbnailImageSize)
									[
										SNew(SImage)
											.Image(PluginIconDynamicImageBrush.IsValid() ? PluginIconDynamicImageBrush.Get() : nullptr)
									]
							]

						+SHorizontalBox::Slot()
							[
								SNew(SVerticalBox)

								+ SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(SHorizontalBox)

										// Friendly name
										+SHorizontalBox::Slot()
											.AutoWidth()
											.VAlign(VAlign_Center)
											.Padding(PaddingAmount)
											[
												SNew(STextBlock)
													.Text(GetPluginNameText())
													.HighlightText_Raw(&OwnerWeak.Pin()->GetOwner().GetPluginTextFilter(), &FPluginTextFilter::GetRawFilterText)
													.TextStyle(FPluginStyle::Get(), "PluginTile.NameText")
											]

										// "NEW!" label
										+ SHorizontalBox::Slot()
											.AutoWidth()
											.Padding(10.0f, 0.0f, 0.0f, 0.0f)
											.HAlign(HAlign_Left)
											.VAlign(VAlign_Center)
											[
												SNew(SBorder)
													.Padding(FMargin(5.0f, 3.0f))
													.BorderImage(FPluginStyle::Get()->GetBrush("PluginTile.NewLabelBackground"))
													[
														SNew(STextBlock)
															.Visibility(FPluginBrowserModule::Get().IsNewlyInstalledPlugin(Plugin->GetName())? EVisibility::Visible : EVisibility::Collapsed)
															.Font(FPluginStyle::Get()->GetFontStyle(TEXT("PluginTile.NewLabelFont")))
															.Text(LOCTEXT("PluginNewLabel", "NEW!"))
															.TextStyle(FPluginStyle::Get(), "PluginTile.NewLabelText")
													]
											]

										// Gap
										+ SHorizontalBox::Slot()
											[
												SNew(SSpacer)
											]

										// Version
										+ SHorizontalBox::Slot()
											.HAlign(HAlign_Right)
											.Padding(PaddingAmount)
											.AutoWidth()
											[
												SNew(SHorizontalBox)

												// noredist/restricted label
												+ SHorizontalBox::Slot()
													.AutoWidth()
													.VAlign(VAlign_Bottom)
													[
														RestrictedPluginWidget
													]

												// beta version label
												+ SHorizontalBox::Slot()
													.AutoWidth()
													.VAlign(VAlign_Bottom)
													[
														SNew(SHorizontalBox)
														.Visibility((PluginDescriptor.bIsBetaVersion || PluginDescriptor.bIsExperimentalVersion) ? EVisibility::Visible : EVisibility::Collapsed)
														+ SHorizontalBox::Slot()
															.AutoWidth()
															.VAlign(VAlign_Bottom)
															.Padding(0.0f, 0.0f, 0.0f, 2.0f)
															[
																SNew(SImage)
																	.Image(FPluginStyle::Get()->GetBrush("PluginTile.BetaWarning"))
															]
														+ SHorizontalBox::Slot()
															.AutoWidth()
															.VAlign(VAlign_Bottom)
															.Padding(2.0f, 0.0f, 8.0f, 1.0f)
															[
																SNew(STextBlock)
																	.TextStyle(FPluginStyle::Get(), "PluginTile.BetaText")
																	.Text(PluginDescriptor.bIsBetaVersion ? LOCTEXT("PluginBetaVersionText", "BETA") : LOCTEXT("PluginExperimentalVersionText", "EXPERIMENTAL"))
															]
													]

												// version number
												+ SHorizontalBox::Slot()
													.AutoWidth()
													.VAlign( VAlign_Bottom )
													.Padding(0.0f, 0.0f, 0.0f, 1.0f) // Lower padding to align font with version number base
													[
														SNew(STextBlock)
															.Text(LOCTEXT("PluginVersionLabel", "Version "))
													]

												+ SHorizontalBox::Slot()
													.AutoWidth()
													.VAlign( VAlign_Bottom )
													.Padding( 0.0f, 0.0f, 2.0f, 0.0f )	// Extra padding from the right edge
													[
														SNew(STextBlock)
															.Text(FText::FromString(PluginDescriptor.VersionName))
															.TextStyle(FPluginStyle::Get(), "PluginTile.VersionNumberText")
													]
											]
									]
			
								+ SVerticalBox::Slot()
									[
										SNew(SVerticalBox)
				
										// Description
										+ SVerticalBox::Slot()
											.Padding( PaddingAmount )
											[
												SNew(STextBlock)
													.Text(FText::FromString(PluginDescriptor.Description))
													.HighlightText_Raw(&OwnerWeak.Pin()->GetOwner().GetPluginTextFilter(), &FPluginTextFilter::GetRawFilterText)
													.AutoWrapText(true)
											]

										+ SVerticalBox::Slot()
											.Padding(PaddingAmount)
											.AutoHeight()
											[
												SNew(SHorizontalBox)

												// Enable checkbox
												+ SHorizontalBox::Slot()
													.Padding(PaddingAmount)
													.HAlign(HAlign_Left)
													[
														SNew(SCheckBox)
															.OnCheckStateChanged(this, &SPluginTile::OnEnablePluginCheckboxChanged)
															.IsChecked(this, &SPluginTile::IsPluginEnabled)
															.ToolTipText(LOCTEXT("EnableDisableButtonToolTip", "Toggles whether this plugin is enabled for your current project.  You may need to restart the program for this change to take effect."))
															.Content()
															[
																SNew(STextBlock)
																	.Text(LOCTEXT("EnablePluginCheckbox", "Enabled"))
															]
													]

												// edit link
												+ SHorizontalBox::Slot()
													.HAlign(HAlign_Center)
													.AutoWidth()
													.Padding(2.0f, 0.0f, 0.0f, 0.0f)
													[
														SNew(SHorizontalBox)

														+ SHorizontalBox::Slot()
														.AutoWidth()
														.Padding(PaddingAmount)
														[
															SNew(SHyperlink)
															.Visibility(this, &SPluginTile::GetAuthoringButtonsVisibility)	
															.OnNavigate(this, &SPluginTile::OnEditPlugin)
															.Text(LOCTEXT("EditPlugin", "Edit..."))
														]

														+ SHorizontalBox::Slot()
														.AutoWidth()
														.Padding(PaddingAmount)
														[
															SNew(SHyperlink)
															.Visibility(this, &SPluginTile::GetAuthoringButtonsVisibility)
															.OnNavigate(this, &SPluginTile::OnPackagePlugin)
															.Text(LOCTEXT("PackagePlugin", "Package..."))
														]
													]

												// support link
												+SHorizontalBox::Slot()
													.Padding(PaddingAmount)
													.HAlign(HAlign_Right)
													[
														SupportWidget.ToSharedRef()
													]

												// docs link
												+ SHorizontalBox::Slot()
													.AutoWidth()
													.Padding(12.0f, PaddingAmount, PaddingAmount, PaddingAmount)
													.HAlign(HAlign_Right)
													[
														DocumentationWidget.ToSharedRef()
													]

												// vendor link
												+ SHorizontalBox::Slot()
													.AutoWidth()
													.Padding(12.0f, PaddingAmount, PaddingAmount, PaddingAmount)
													.HAlign(HAlign_Right)
													[
														CreatedByWidget.ToSharedRef()
													]
											]
									]
							]
					]
			]
	];
}


ECheckBoxState SPluginTile::IsPluginEnabled() const
{
	FPluginBrowserModule& PluginBrowserModule = FPluginBrowserModule::Get();
	if(PluginBrowserModule.HasPluginPendingEnable(Plugin->GetName()))
	{
		return PluginBrowserModule.GetPluginPendingEnableState(Plugin->GetName()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	else
	{
		return Plugin->IsEnabled()? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
}

void FindPluginDependencies(const FString& Name, TSet<FString>& Dependencies, TMap<FString, IPlugin*>& NameToPlugin)
{
	IPlugin* Plugin = NameToPlugin.FindRef(Name);
	if (Plugin != nullptr)
	{
		for (const FPluginReferenceDescriptor& Reference : Plugin->GetDescriptor().Plugins)
		{
			if (Reference.bEnabled && !Dependencies.Contains(Reference.Name))
			{
				Dependencies.Add(Reference.Name);
				FindPluginDependencies(Reference.Name, Dependencies, NameToPlugin);
			}
		}
	}
}

void SPluginTile::OnEnablePluginCheckboxChanged(ECheckBoxState NewCheckedState)
{
	const bool bNewEnabledState = NewCheckedState == ECheckBoxState::Checked;

	const FPluginDescriptor& PluginDescriptor = Plugin->GetDescriptor();

	if (bNewEnabledState)
	{
		// If this is plugin is marked as beta, make sure the user is aware before enabling it.
		if (PluginDescriptor.bIsBetaVersion)
		{
			FText WarningMessage = FText::Format(LOCTEXT("Warning_EnablingBetaPlugin", "Plugin '{0}' is a beta version and might be unstable or removed without notice. Please use with caution. Are you sure you want to enable the plugin?"), GetPluginNameText());
			if (EAppReturnType::No == FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage))
			{
				return;
			}
		}
	}
	else
	{
		// Get all the plugins we know about
		TArray<TSharedRef<IPlugin>> EnabledPlugins = IPluginManager::Get().GetEnabledPlugins();

		// Build a map of plugin by name
		TMap<FString, IPlugin*> NameToPlugin;
		for (TSharedRef<IPlugin>& EnabledPlugin : EnabledPlugins)
		{
			NameToPlugin.FindOrAdd(EnabledPlugin->GetName()) = &(EnabledPlugin.Get());
		}

		// Find all the plugins which are dependent on this plugin
		TArray<FString> DependentPluginNames;
		for (TSharedRef<IPlugin>& EnabledPlugin : EnabledPlugins)
		{
			FString EnabledPluginName = EnabledPlugin->GetName();

			TSet<FString> Dependencies;
			FindPluginDependencies(EnabledPluginName, Dependencies, NameToPlugin);

			if (Dependencies.Num() > 0 && Dependencies.Contains(Plugin->GetName()))
			{
				FText Caption = LOCTEXT("DisableDependenciesCaption", "Disable Dependencies");
				FText Message = FText::Format(LOCTEXT("DisableDependenciesMessage", "This plugin is required by {0}. Would you like to disable it as well?"), FText::FromString(EnabledPluginName));
				if (FMessageDialog::Open(EAppMsgType::YesNo, Message, &Caption) == EAppReturnType::No)
				{
					return;
				}
				DependentPluginNames.Add(EnabledPluginName);
			}
		}

		// Disable all the dependent plugins too
		for (const FString& DependentPluginName : DependentPluginNames)
		{
			FText FailureMessage;
			if (!IProjectManager::Get().SetPluginEnabled(DependentPluginName, false, FailureMessage))
			{
				FMessageDialog::Open(EAppMsgType::Ok, FailureMessage);
			}

			TSharedPtr<IPlugin> DependentPlugin = IPluginManager::Get().FindPlugin(DependentPluginName);
			if (DependentPlugin.IsValid())
			{
				FPluginBrowserModule::Get().SetPluginPendingEnableState(DependentPluginName, DependentPlugin->IsEnabled(), false);
			}
		}
	}

	// Finally, enable/disable the plugin we selected
	FText FailMessage;
	bool bSuccess = IProjectManager::Get().SetPluginEnabled(Plugin->GetName(), bNewEnabledState, FailMessage);

	if (bSuccess && IProjectManager::Get().IsCurrentProjectDirty())
	{
		FGameProjectGenerationModule::Get().TryMakeProjectFileWriteable(FPaths::GetProjectFilePath());
		bSuccess = IProjectManager::Get().SaveCurrentProjectToDisk(FailMessage);
	}

	if (bSuccess)
	{
		FPluginBrowserModule::Get().SetPluginPendingEnableState(Plugin->GetName(), Plugin->IsEnabled(), bNewEnabledState);
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, FailMessage);
	}
}

EVisibility SPluginTile::GetAuthoringButtonsVisibility() const
{
	if (FApp::IsEngineInstalled() && Plugin->GetLoadedFrom() == EPluginLoadedFrom::Engine)
	{
		return EVisibility::Hidden;
	}
	if (FApp::IsInstalled() && Plugin->GetType() != EPluginType::Mod)
	{
		return EVisibility::Hidden;
	}
	return EVisibility::Visible;
}

void SPluginTile::OnEditPlugin()
{
	FPluginBrowserModule::Get().OpenPluginEditor(Plugin.ToSharedRef(), OwnerWeak.Pin(), FSimpleDelegate::CreateRaw(this, &SPluginTile::OnEditPluginFinished));
}

void SPluginTile::OnEditPluginFinished()
{
	// Recreate the widgets on this tile
	RecreateWidgets();

	// Refresh the parent too
	if(OwnerWeak.IsValid())
	{
		OwnerWeak.Pin()->GetOwner().SetNeedsRefresh();
	}
}

void SPluginTile::OnPackagePlugin()
{
	FString DefaultDirectory;
	FString OutputDirectory;

	if ( !FDesktopPlatformModule::Get()->OpenDirectoryDialog(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()), LOCTEXT("PackagePluginDialogTitle", "Package Plugin...").ToString(), DefaultDirectory, OutputDirectory) )
	{
		return;
	}

	// Ensure path is full rather than relative (for macs)
	FString DescriptorFilename = Plugin->GetDescriptorFileName();
	FString DescriptorFullPath = FPaths::ConvertRelativePathToFull(DescriptorFilename);
	OutputDirectory = FPaths::Combine(OutputDirectory, Plugin->GetName());
	FString CommandLine = FString::Printf(TEXT("BuildPlugin -Plugin=\"%s\" -Package=\"%s\" -CreateSubFolder"), *DescriptorFullPath, *OutputDirectory);

#if PLATFORM_WINDOWS
	FText PlatformName = LOCTEXT("PlatformName_Windows", "Windows");
#elif PLATFORM_MAC
	FText PlatformName = LOCTEXT("PlatformName_Mac", "Mac");
#elif PLATFORM_LINUX
	FText PlatformName = LOCTEXT("PlatformName_Linux", "Linux");
#else
	FText PlatformName = LOCTEXT("PlatformName_Other", "Other OS");
#endif

	IUATHelperModule::Get().CreateUatTask(CommandLine, PlatformName, LOCTEXT("PackagePluginTaskName", "Packaging Plugin"),
		LOCTEXT("PackagePluginTaskShortName", "Package Plugin Task"), FEditorStyle::GetBrush(TEXT("MainFrame.CookContent")));
}

#undef LOCTEXT_NAMESPACE
