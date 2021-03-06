// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/SlateDelegates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboButton.h"

class UNREALED_API SEditorHeaderButton : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SEditorHeaderButton) :
		_Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
		{}

		/** The text to display in the button. */
		SLATE_ATTRIBUTE(FText, Text)

		SLATE_ATTRIBUTE(const FSlateBrush*, Icon)

		/** The clicked handler. Note that if this is set, the button will behave as though it were just a button.
		 * This means that OnGetMenuContent, OnComboBoxOpened and OnMenuOpenChanged will all be ignored, since there is no menu.
		 */
		SLATE_EVENT(FOnClicked, OnClicked)

		/** The static menu content widget. */
		SLATE_NAMED_SLOT(FArguments, MenuContent)

		SLATE_EVENT(FOnGetContent, OnGetMenuContent)
		SLATE_EVENT(FOnComboBoxOpened, OnComboBoxOpened)
		SLATE_EVENT(FOnIsOpenChanged, OnMenuOpenChanged)

	SLATE_END_ARGS()

	SEditorHeaderButton() {}

	void Construct(const FArguments& InArgs);

	void SetMenuContentWidgetToFocus(TWeakPtr<SWidget> Widget);
	void SetIsMenuOpen(bool bIsOpen, bool bIsFocused);

private:

	TSharedPtr<SComboButton> ComboButton;
	TSharedPtr<class SButton> Button;
};