// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOptimusEditorGraphNode.h"

#include "OptimusEditorGraph.h"
#include "OptimusEditorGraphNode.h"
#include "OptimusEditorStyle.h"

#include "OptimusActionStack.h"
#include "OptimusNode.h"
#include "OptimusNodeGraph.h"
#include "OptimusNodePin.h"

#include "Editor.h"
#include "GraphEditorSettings.h"
#include "SGraphPin.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Views/STreeView.h"


#define LOCTEXT_NAMESPACE "SOptimusEditorGraphNode"

static const FName NAME_Pin_Resource_Connected("Optimus.Node.Pin.Resource_Connected");
static const FName NAME_Pin_Resource_Disconnected("Optimus.Node.Pin.Resource_Disconnected");
static const FName NAME_Pin_Value_Connected("Optimus.Node.Pin.Value_Connected");
static const FName NAME_Pin_Value_Disconnected("Optimus.Node.Pin.Value_Disconnected");

static const FSlateBrush* CachedImg_Pin_Resource_Connected = nullptr;
static const FSlateBrush* CachedImg_Pin_Resource_Disconnected = nullptr;
static const FSlateBrush* CachedImg_Pin_Value_Connected = nullptr;
static const FSlateBrush* CachedImg_Pin_Value_Disconnected = nullptr;


class SOptimusEditorExpanderArrow : public SExpanderArrow
{
	SLATE_BEGIN_ARGS(SOptimusEditorExpanderArrow) {}

	SLATE_ARGUMENT(bool, LeftAligned)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<class ITableRow>& TableRow)
	{
		bLeftAligned = InArgs._LeftAligned;

		SExpanderArrow::Construct(
		    SExpanderArrow::FArguments()
		        .IndentAmount(8.0f),
		    TableRow);

		// override padding
		ChildSlot.Padding(TAttribute<FMargin>(this, &SOptimusEditorExpanderArrow::GetExpanderPadding_Extended));

		// override image
		ExpanderArrow->SetContent(
		    SNew(SImage)
		        .Image(this, &SOptimusEditorExpanderArrow::GetExpanderImage_Extended)
		        .ColorAndOpacity(FSlateColor::UseForeground()));
	}

	FMargin GetExpanderPadding_Extended() const
	{
		const int32 NestingDepth = FMath::Max(0, OwnerRowPtr.Pin()->GetIndentLevel() - BaseIndentLevel.Get());
		const float Indent = IndentAmount.Get(8.0f);
		return bLeftAligned ? FMargin(NestingDepth * Indent, 0, 0, 0) : FMargin(0, 0, NestingDepth * Indent, 0);
	}

	const FSlateBrush* GetExpanderImage_Extended() const
	{
		const bool bIsItemExpanded = OwnerRowPtr.Pin()->IsItemExpanded();

		// FIXME: COllapse to a table.
		FName ResourceName;
		if (bIsItemExpanded)
		{
			if (ExpanderArrow->IsHovered())
			{
				static FName ExpandedHoveredLeftName("Optimus.Node.PinTree.Arrow_Expanded_Hovered_Left");
				static FName ExpandedHoveredRightName("Optimus.Node.PinTree.Arrow_Expanded_Hovered_Right");
				ResourceName = bLeftAligned ? ExpandedHoveredLeftName : ExpandedHoveredRightName;
			}
			else
			{
				static FName ExpandedLeftName("Optimus.Node.PinTree.Arrow_Expanded_Left");
				static FName ExpandedRightName("Optimus.Node.PinTree.Arrow_Expanded_Right");
				ResourceName = bLeftAligned ? ExpandedLeftName : ExpandedRightName;
			}
		}
		else
		{
			if (ExpanderArrow->IsHovered())
			{
				static FName CollapsedHoveredLeftName("Optimus.Node.PinTree.Arrow_Collapsed_Hovered_Left");
				static FName CollapsedHoveredRightName("Optimus.Node.PinTree.Arrow_Collapsed_Hovered_Right");
				ResourceName = bLeftAligned ? CollapsedHoveredLeftName : CollapsedHoveredRightName;
			}
			else
			{
				static FName CollapsedLeftName("Optimus.Node.PinTree.Arrow_Collapsed_Left");
				static FName CollapsedRightName("Optimus.Node.PinTree.Arrow_Collapsed_Right");
				ResourceName = bLeftAligned ? CollapsedLeftName : CollapsedRightName;
			}
		}

		return FOptimusEditorStyle::Get().GetBrush(ResourceName);
	}

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
	{
		return FCursorReply::Cursor(EMouseCursor::Default);
	}

	bool bLeftAligned;
};


class SOptimusEditorGraphPinTreeRow : public STableRow<UOptimusNodePin*>
{
	SLATE_BEGIN_ARGS(SOptimusEditorGraphPinTreeRow) {}

	SLATE_ARGUMENT(bool, LeftAligned)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		bLeftAligned = InArgs._LeftAligned;

		STableRow<UOptimusNodePin*>::Construct(STableRow<UOptimusNodePin*>::FArguments(), InOwnerTableView);
	}

	const FSlateBrush* GetBorder() const
	{
		// We want a transparent background.
		return FCoreStyle::Get().GetBrush("NoBrush");
	}


	void ConstructChildren( ETableViewMode::Type InOwnerTableMode, const TAttribute<FMargin>& InPadding, const TSharedRef<SWidget>& InContent ) override
	{
		const UGraphEditorSettings* Settings = GetDefault<UGraphEditorSettings>();
		FMargin InputPadding = Settings->GetInputPinPadding();
		InputPadding.Top = InputPadding.Bottom = 3.0f;
		InputPadding.Right = 0.0f;

		FMargin OutputPadding = Settings->GetOutputPinPadding();
		OutputPadding.Top = OutputPadding.Bottom = 3.0f;
		OutputPadding.Left = 2.0f;

		this->Content = InContent;

		SHorizontalBox::FSlot* InnerContentSlotNativePtr = nullptr;

		TSharedRef<SHorizontalBox> ContentBox = SNew(SHorizontalBox);

		if(bLeftAligned)
		{
			ContentBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(InputPadding)
			[
				SAssignNew(PinContentBox, SBox)
			];

			ContentBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SOptimusEditorExpanderArrow, SharedThis(this))
				.LeftAligned(bLeftAligned)
			];

			ContentBox->AddSlot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(2.0f)
			.Expose(InnerContentSlotNativePtr)
			[
				SAssignNew(LabelContentBox, SBox)
				[
					InContent
				]
			];
		}
		else
		{
			ContentBox->AddSlot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(2.0f)
			.Expose(InnerContentSlotNativePtr)
			[
				SAssignNew(LabelContentBox, SBox)
				[
					InContent
				]
			];

			ContentBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SOptimusEditorExpanderArrow, SharedThis(this))
				.LeftAligned(bLeftAligned)
			];

			ContentBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(OutputPadding)
			[
				SAssignNew(PinContentBox, SBox)
			];
		}

		this->ChildSlot
		[
			ContentBox
		];

		InnerContentSlot = InnerContentSlotNativePtr;
	}

	/** Exposed boxes to slot pin widgets into */
	TSharedPtr<SBox> PinContentBox;
	TSharedPtr<SBox> LabelContentBox;

	/** Whether we align our content left or right */
	bool bLeftAligned;
};


static void SetTreeExpansion_Recursive(
	TSharedPtr<STreeView<UOptimusNodePin*>>& InTreeWidget, 
	const TArray<UOptimusNodePin*> &InItems
	)
{
	for (UOptimusNodePin *Pin: InItems)
	{
		if (Pin->GetIsExpanded())
		{
			InTreeWidget->SetItemExpansion(Pin, true);

			SetTreeExpansion_Recursive(InTreeWidget, Pin->GetSubPins());
		}
	}
}

void SOptimusEditorGraphNode::Construct(const FArguments& InArgs)
{
	if (!CachedImg_Pin_Resource_Connected)
	{
		CachedImg_Pin_Resource_Connected = FOptimusEditorStyle::Get().GetBrush(NAME_Pin_Resource_Connected);
		CachedImg_Pin_Resource_Disconnected = FOptimusEditorStyle::Get().GetBrush(NAME_Pin_Resource_Disconnected);
		CachedImg_Pin_Value_Connected = FOptimusEditorStyle::Get().GetBrush(NAME_Pin_Value_Connected);
		CachedImg_Pin_Value_Disconnected = FOptimusEditorStyle::Get().GetBrush(NAME_Pin_Value_Disconnected);
	}

	GraphNode = InArgs._GraphNode;

	UOptimusEditorGraphNode *EditorGraphNode = InArgs._GraphNode;

	SetCursor( EMouseCursor::CardinalCross );
	UpdateGraphNode();

	TreeScrollBar = SNew(SScrollBar);

	LeftNodeBox->AddSlot()
	    .AutoHeight()
		[
			SAssignNew(InputTree, STreeView<UOptimusNodePin*>)
	        .Visibility(this, &SOptimusEditorGraphNode::GetInputTreeVisibility)
	        .TreeItemsSource(EditorGraphNode->GetTopLevelInputPins())
	        .SelectionMode(ESelectionMode::None)
	        .OnGenerateRow(this, &SOptimusEditorGraphNode::MakeTableRowWidget)
	        .OnGetChildren(this, &SOptimusEditorGraphNode::HandleGetChildrenForTree)
	        .OnExpansionChanged(this, &SOptimusEditorGraphNode::HandleExpansionChanged)
	        .ExternalScrollbar(TreeScrollBar)
	        .ItemHeight(20.0f)
		];

	RightNodeBox->AddSlot()
	    .AutoHeight()
		[
			SAssignNew(OutputTree, STreeView<UOptimusNodePin*>)
			.Visibility(this, &SOptimusEditorGraphNode::GetOutputTreeVisibility)
			.TreeItemsSource(EditorGraphNode->GetTopLevelOutputPins())
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(this, &SOptimusEditorGraphNode::MakeTableRowWidget)
			.OnGetChildren(this, &SOptimusEditorGraphNode::HandleGetChildrenForTree)
			.OnExpansionChanged(this, &SOptimusEditorGraphNode::HandleExpansionChanged)
			.ExternalScrollbar(TreeScrollBar)
			.ItemHeight(20.0f)
		];

	// FIXME: Do expansion from stored expansion data.
	SetTreeExpansion_Recursive(InputTree, *EditorGraphNode->GetTopLevelInputPins());
	SetTreeExpansion_Recursive(OutputTree, *EditorGraphNode->GetTopLevelOutputPins());
}


void SOptimusEditorGraphNode::EndUserInteraction() const
{
	UOptimusEditorGraph* Graph = Cast<UOptimusEditorGraph>(GraphNode->GetGraph());
	if (ensure(Graph))
	{
#if WITH_EDITOR
		// Cancel the current transaction created by SNodePanel::OnMouseMove so that the
		// only transaction recorded is the one we place on the action stack.
		if (GEditor)
		{
			GEditor->CancelTransaction(0);
		}
#endif

		const TSet<UOptimusEditorGraphNode*> &SelectedNodes = Graph->GetSelectedNodes();

		if (SelectedNodes.Num() == 0)
		{
			return;
		}

		FString ActionTitle;
		if (SelectedNodes.Num() == 1)
		{
			ActionTitle = TEXT("Move Node");
		}
		else
		{
			ActionTitle = FString::Printf(TEXT("Move %d Nodes"), SelectedNodes.Num());
		}

		FOptimusActionScope Scope(*Graph->GetModelGraph()->GetActionStack(), ActionTitle);
		for (UOptimusEditorGraphNode* SelectedNode : SelectedNodes)
		{
			FVector2D Position(SelectedNode->NodePosX, SelectedNode->NodePosY);
			SelectedNode->ModelNode->SetGraphPosition(Position);
		}
	}
}


void SOptimusEditorGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetShowLabel(false);

	UOptimusEditorGraphNode* EditorGraphNode = GetEditorGraphNode();
	if (ensure(EditorGraphNode))
	{
		const UEdGraphPin* EdPinObj = PinToAdd->GetPinObj();

		UOptimusNodePin *ModelPin = EditorGraphNode->FindModelPinFromGraphPin(EdPinObj);
		if (ModelPin)
		{
			switch (ModelPin->GetStorageType())
			{
			case EOptimusNodePinStorageType::Resource:
				PinToAdd->SetCustomPinIcon(CachedImg_Pin_Resource_Connected, CachedImg_Pin_Resource_Disconnected);
				break;

			case EOptimusNodePinStorageType::Value:
				PinToAdd->SetCustomPinIcon(CachedImg_Pin_Value_Connected, CachedImg_Pin_Value_Disconnected);
				break;
			}
		}

		// Remove value widget from combined pin content
		TSharedPtr<SWrapBox> LabelAndValueWidget = PinToAdd->GetLabelAndValue();
		TSharedPtr<SHorizontalBox> FullPinHorizontalRowWidget = PinToAdd->GetFullPinHorizontalRowWidget().Pin();
		if (LabelAndValueWidget.IsValid() && FullPinHorizontalRowWidget.IsValid())
		{
			FullPinHorizontalRowWidget->RemoveSlot(LabelAndValueWidget.ToSharedRef());
		}

		PinToAdd->SetOwner(SharedThis(this));
		PinWidgetMap.Add(EdPinObj, PinToAdd);
		if (EdPinObj->Direction == EGPD_Input)
		{
			InputPins.Add(PinToAdd);
		}
		else
		{
			OutputPins.Add(PinToAdd);
		}
	}
}


//const FSlateBrush* SOptimusEditorGraphNode::GetNodeBodyBrush() const
//{
//	return FEditorStyle::GetBrush("Graph.Node.TintedBody");
//}


TSharedPtr<SGraphPin> SOptimusEditorGraphNode::GetHoveredPin(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	TSharedPtr<SGraphPin> HoveredPin = SGraphNode::GetHoveredPin(MyGeometry, MouseEvent);
#if 0
	if (!HoveredPin.IsValid())
	{
		TArray<TSharedRef<SWidget>> ExtraWidgetArray;
		ExtraWidgetToPinMap.GenerateKeyArray(ExtraWidgetArray);
		TSet<TSharedRef<SWidget>> ExtraWidgets(ExtraWidgetArray);

		TMap<TSharedRef<SWidget>, FArrangedWidget> Result;
		FindChildGeometries(MyGeometry, ExtraWidgets, Result);

		if (Result.Num() > 0)
		{
			FArrangedChildren ArrangedWidgets(EVisibility::Visible);
			Result.GenerateValueArray(ArrangedWidgets.GetInternalArray());
			int32 HoveredWidgetIndex = SWidget::FindChildUnderMouse(ArrangedWidgets, MouseEvent);
			if (HoveredWidgetIndex != INDEX_NONE)
			{
				return *ExtraWidgetToPinMap.Find(ArrangedWidgets[HoveredWidgetIndex].Widget);
			}
		}
	}
#endif
	return HoveredPin;
}


UOptimusEditorGraphNode* SOptimusEditorGraphNode::GetEditorGraphNode() const
{
	return Cast<UOptimusEditorGraphNode>(GraphNode);
}


UOptimusNode* SOptimusEditorGraphNode::GetModelNode() const
{
	UOptimusEditorGraphNode* EditorGraphNode = GetEditorGraphNode();
	return EditorGraphNode ? EditorGraphNode->ModelNode : nullptr;
}


EVisibility SOptimusEditorGraphNode::GetInputTreeVisibility() const
{
	const UOptimusEditorGraphNode* EditorGraphNode = GetEditorGraphNode();
	
	return EditorGraphNode && !EditorGraphNode->GetTopLevelInputPins()->IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SOptimusEditorGraphNode::GetOutputTreeVisibility() const
{
	const UOptimusEditorGraphNode* EditorGraphNode = GetEditorGraphNode();

	return EditorGraphNode && !EditorGraphNode->GetTopLevelOutputPins()->IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
}


TSharedRef<ITableRow> SOptimusEditorGraphNode::MakeTableRowWidget(
	UOptimusNodePin* InModelPin, 
	const TSharedRef<STableViewBase>& OwnerTable
	)
{
	const bool bIsLeaf = InModelPin->GetSubPins().IsEmpty();
	const bool bIsInput = InModelPin->GetDirection() == EOptimusNodePinDirection::Input;
	const bool bIsValue = InModelPin->GetStorageType() == EOptimusNodePinStorageType::Value;

	TSharedRef<SOptimusEditorGraphPinTreeRow> TreeRow = SNew(SOptimusEditorGraphPinTreeRow, OwnerTable)
		.LeftAligned(bIsInput)
		// .ToolTipText(InItem->GetTooltipText())
		;

	UOptimusEditorGraphNode* EditorGraphNode = GetEditorGraphNode();
	TSharedPtr<SGraphPin> PinWidget;
	if (ensure(EditorGraphNode))
	{
		UEdGraphPin* GraphPin = EditorGraphNode->FindGraphPinFromModelPin(InModelPin);
		TSharedPtr<SGraphPin> *PinWidgetPtr = PinWidgetMap.Find(GraphPin);
		if (PinWidgetPtr)
		{
			PinWidget = *PinWidgetPtr;
		}
	}

	if (PinWidget.IsValid())
	{
		TWeakPtr<SGraphPin> WeakPin = PinWidget;
		TSharedRef<SWidget> LabelWidget = SNew(STextBlock)
			.Text(this, &SOptimusEditorGraphNode::GetPinLabel, WeakPin)
			.TextStyle(FEditorStyle::Get(), NAME_DefaultPinLabelStyle)
		    .ColorAndOpacity(FLinearColor::White)
			// .ColorAndOpacity(this, &SOptimusEditorGraphNode::GetPinTextColor, WeakPin)
			;

		TSharedPtr<SWidget> InputValueWidget;
		if (bIsLeaf && bIsInput && bIsValue)
		{
			InputValueWidget = PinWidget->GetValueWidget();
		}

		if (InputValueWidget.IsValid())
		{
			TSharedRef<SWidget> LabelAndInputWidget = 
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(2.0f)
				[
					LabelWidget
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(2.0f, 2.0f, 18.0f, 2.0f)
				[
					InputValueWidget.IsValid() ? InputValueWidget.ToSharedRef() : SNew(SSpacer)
				];

			TreeRow->LabelContentBox->SetContent(LabelAndInputWidget);
		}
		else
		{
			TreeRow->LabelContentBox->SetContent(LabelWidget);
		}

		// To allow the label to be a part of the hoverable set of widgets for the pin.
		// HoverWidgetLabels.Add(LabelWidget);
		// HoverWidgetPins.Add(PinWidget.ToSharedRef());

		TreeRow->PinContentBox->SetContent(PinWidget.ToSharedRef());
	}

	return TreeRow;
}


void SOptimusEditorGraphNode::HandleGetChildrenForTree(
	UOptimusNodePin* InItem, 
	TArray<UOptimusNodePin*>& OutChildren
	)
{
	OutChildren.Append(InItem->GetSubPins());
}


void SOptimusEditorGraphNode::HandleExpansionChanged(
	UOptimusNodePin* InItem, 
	bool bExpanded
	)
{
	InItem->SetIsExpanded(bExpanded);
}


FText SOptimusEditorGraphNode::GetPinLabel(TWeakPtr<SGraphPin> InWeakGraphPin) const
{
	UOptimusEditorGraphNode* EditorGraphNode = GetEditorGraphNode();
	TSharedPtr<SGraphPin> GraphPin = InWeakGraphPin.Pin();

	if (GraphPin.IsValid() && EditorGraphNode)
	{
		return EditorGraphNode->GetPinDisplayName(GraphPin->GetPinObj());
	}
	return FText::GetEmpty();
}


#undef LOCTEXT_NAMESPACE
