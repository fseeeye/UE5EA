// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "EditorDialogLibrary.generated.h"

/**
 * Utility class to create simple pop-up dialogs to notify the user of task completion, 
 * or to ask them to make simple Yes/No/Retry/Cancel type decisions.
 */
UCLASS(meta=(ScriptName="EditorDialog"))
class EDITORSCRIPTINGUTILITIES_API UEditorDialogLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Open a modal message box dialog with the given message. If running in "-unattended" mode it will immediately 
	 * return the value specified by DefaultValue. If not running in "-unattended" mode then it will block execution
	 * until the user makes a decision, at which point their decision will be returned.
	 * @param Title 		The title of the created dialog window
	 * @param Message 		Text of the message to show
	 * @param MessageType 	Specifies which buttons the dialog should have
	 * @param DefaultValue 	If the application is Unattended, the function will log and return DefaultValue
	 * @return The result of the users decision, or DefaultValue if running in unattended mode.
	*/
	UFUNCTION(BlueprintCallable, DisplayName = "Show Message Dialog", Category = "Editor Scripting | Message Dialog")
	static TEnumAsByte<EAppReturnType::Type> ShowMessage(const FText& Title, const FText& Message, TEnumAsByte<EAppMsgType::Type> MessageType, TEnumAsByte<EAppReturnType::Type> DefaultValue = EAppReturnType::Type::No);

	/**
	 * Open a modal message box dialog containing a details view for inspecting / modifying a UObject. 
	 * @param Title 		The title of the created dialog window
	 * @param InOutObject 	Object instance of ClassOfObject which is supposed to be viewed
	 * @return The result of the users decision, true=Ok, false=Cancel, or false if running in unattended mode.
	*/
	UFUNCTION(BlueprintCallable, DisplayName = "Show Object Dialog", Category = "Editor Scripting | Object Dialog")
	static bool ShowObjectDetailsView(const FText& Title, UObject* InOutObject);

	/**
	 * Open a modal message box dialog containing a details view for inspecting / modifying multiples UObjects. 
	 * @param Title 		The title of the created dialog window
	 * @param InOutObjects 	Array of object instances which are supposed to be viewed
	 * @return The result of the users decision, true=Ok, false=Cancel, or false if running in unattended mode.
	*/
	UFUNCTION(BlueprintCallable, DisplayName = "Show Objects Dialog", Category = "Editor Scripting | Objects Dialog")
	static bool ShowObjectsDetailsView(const FText& Title, const TArray<UObject*>& InOutObjects);
};

