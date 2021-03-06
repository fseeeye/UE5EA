// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"


/// Optimus editor command set.
class FOptimusEditorCommands : public TCommands<FOptimusEditorCommands>
{
public:
	FOptimusEditorCommands();

	// TCommands<> overrides
	void RegisterCommands() override;

	/// FIXME: Dummy command to carve in a toolbar.
	TSharedPtr< FUICommandInfo > Apply;
};
