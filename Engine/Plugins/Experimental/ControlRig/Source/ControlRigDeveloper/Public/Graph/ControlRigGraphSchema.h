// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphSchema.h"
#include "GraphEditorDragDropAction.h"
#include "ControlRigGraphSchema.generated.h"

class UControlRigBlueprint;
class UControlRigGraph;
class UControlRigGraphNode;
class UControlRigGraphNode_Unit;
class UControlRigGraphNode_Property;

/** Extra operations that can be performed on pin connection */
enum class ECanCreateConnectionResponse_Extended
{
	None,

	BreakChildren,

	BreakParent,
};

/** Struct used to extend the response to a conneciton request to include breaking parents/children */
struct FControlRigPinConnectionResponse
{
	FControlRigPinConnectionResponse(const ECanCreateConnectionResponse InResponse, FText InMessage, ECanCreateConnectionResponse_Extended InExtendedResponse = ECanCreateConnectionResponse_Extended::None)
		: Response(InResponse, MoveTemp(InMessage))
		, ExtendedResponse(InExtendedResponse)
	{
	}

	friend bool operator==(const FControlRigPinConnectionResponse& A, const FControlRigPinConnectionResponse& B)
	{
		return (A.Response == B.Response) && (A.ExtendedResponse == B.ExtendedResponse);
	}	

	FPinConnectionResponse Response;
	ECanCreateConnectionResponse_Extended ExtendedResponse;
};

/** DragDropAction class for drag and dropping an item from the My Blueprints tree (e.g., variable or function) */
class CONTROLRIGDEVELOPER_API FControlRigFunctionDragDropAction : public FGraphSchemaActionDragDropAction
{
public:

	DRAG_DROP_OPERATOR_TYPE(FControlRigFunctionDragDropAction, FGraphSchemaActionDragDropAction)

	// FGraphEditorDragDropAction interface
	virtual FReply DroppedOnPanel(const TSharedRef< class SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;
	virtual FReply DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	virtual FReply DroppedOnAction(TSharedRef<FEdGraphSchemaAction> Action) override;
	virtual FReply DroppedOnCategory(FText Category) override;
	virtual void HoverTargetChanged() override;
	// End of FGraphEditorDragDropAction

	/** Set if operation is modified by alt */
	void SetAltDrag(bool InIsAltDrag) { bAltDrag = InIsAltDrag; }

	/** Set if operation is modified by the ctrl key */
	void SetCtrlDrag(bool InIsCtrlDrag) { bControlDrag = InIsCtrlDrag; }

protected:

	/** Constructor */
	FControlRigFunctionDragDropAction();

	static TSharedRef<FControlRigFunctionDragDropAction> New(TSharedPtr<FEdGraphSchemaAction> InAction, UControlRigBlueprint* InRigBlueprint, UControlRigGraph* InRigGraph);

protected:

	UControlRigBlueprint* SourceRigBlueprint;
	UControlRigGraph* SourceRigGraph;
	bool bControlDrag;
	bool bAltDrag;

	friend class UControlRigGraphSchema;
};

UCLASS()
class CONTROLRIGDEVELOPER_API UControlRigGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	/** Name constants */
	static const FName GraphName_ControlRig;

public:
	UControlRigGraphSchema();

	// UEdGraphSchema interface
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual void GetContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual bool TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const override;
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;
	virtual bool CanGraphBeDropped(TSharedPtr<FEdGraphSchemaAction> InAction) const override;
	virtual FReply BeginGraphDragAction(TSharedPtr<FEdGraphSchemaAction> InAction) const override;
	virtual class FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const override;
	virtual bool ShouldHidePinDefaultValue(UEdGraphPin* Pin) const override;
	virtual void TrySetDefaultValue(UEdGraphPin& InPin, const FString& InNewDefaultValue, bool bMarkAsModified = true) const override;
	virtual void TrySetDefaultObject(UEdGraphPin& InPin, UObject* InNewDefaultObject, bool bMarkAsModified) const override;
	virtual void TrySetDefaultText(UEdGraphPin& InPin, const FText& InNewDefaultText, bool bMarkAsModified) const override;
	virtual bool ShouldAlwaysPurgeOnModification() const override { return false; }
	virtual bool ArePinsCompatible(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UClass* CallingContext, bool bIgnoreArray /*= false*/) const override;
	virtual bool DoesSupportPinWatching() const	override { return true; }
	virtual bool IsPinBeingWatched(UEdGraphPin const* Pin) const override;
	virtual void ClearPinWatch(UEdGraphPin const* Pin) const override;
	virtual void OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const override;
	virtual bool MarkBlueprintDirtyFromNewNode(UBlueprint* InBlueprint, UEdGraphNode* InEdGraphNode) const override;
	virtual bool SafeDeleteNodeFromGraph(UEdGraph* Graph, UEdGraphNode* Node) const override;
	virtual bool CanVariableBeDropped(UEdGraph* InGraph, FProperty* InVariableToDrop) const override;
	virtual bool RequestVariableDropOnPanel(UEdGraph* InGraph, FProperty* InVariableToDrop, const FVector2D& InDropPosition, const FVector2D& InScreenPosition) override;
	virtual bool RequestVariableDropOnPin(UEdGraph* InGraph, FProperty* InVariableToDrop, UEdGraphPin* InPin, const FVector2D& InDropPosition, const FVector2D& InScreenPosition) override;
	virtual bool IsStructEditable(UStruct* InStruct) const;
	virtual void SetNodePosition(UEdGraphNode* Node, const FVector2D& Position) const override;
	virtual void GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const override;
	virtual FText GetGraphCategory(const UEdGraph* InGraph) const override;
	virtual FReply TrySetGraphCategory(const UEdGraph* InGraph, const FText& InCategory) override;
	virtual bool TryDeleteGraph(UEdGraph* GraphToDelete) const override;
	virtual bool TryRenameGraph(UEdGraph* GraphToRename, const FName& InNewName) const override;
	virtual bool CanDuplicateGraph(UEdGraph* InSourceGraph) const { return false; }
	virtual UEdGraphPin* DropPinOnNode(UEdGraphNode* InTargetNode, const FName& InSourcePinName, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection) const override;
	virtual bool SupportsDropPinOnNode(UEdGraphNode* InTargetNode, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection, FText& OutErrorMessage) const override;
	virtual void SetPinBeingDroppedOnNode(UEdGraphPin* InSourcePin) const override { PinBeingDropped = InSourcePin; }

	/** Create a graph node for a rig */
	UControlRigGraphNode* CreateGraphNode(UControlRigGraph* InGraph, const FName& InPropertyName) const;

	/** Automatically layout the passed-in nodes */
	void LayoutNodes(UControlRigGraph* InGraph, const TArray<UControlRigGraphNode*>& InNodes) const;

	/** Helper function to rename a node */
	void RenameNode(UControlRigGraphNode* Node, const FName& InNewNodeName) const;

	/** Helper function to recursively reset the pin defaults */
	virtual void ResetPinDefaultsRecursive(UEdGraphPin* InPin) const;

	/** Returns all of the applicable pin types for variables within a control rig */
	virtual void GetVariablePinTypes(TArray<FEdGraphPinType>& PinTypes) const;

	void EndGraphNodeInteraction(UEdGraphNode* InNode) const;

private:

	const UEdGraphPin* LastPinForCompatibleCheck = nullptr;
	bool bLastPinWasInput;
	mutable UEdGraphPin* PinBeingDropped = nullptr;

	friend class UControlRigRerouteNodeSpawner;
	friend class UControlRigIfNodeSpawner;
	friend class UControlRigSelectNodeSpawner;
	friend class UControlRigUnitNodeSpawner;
};

