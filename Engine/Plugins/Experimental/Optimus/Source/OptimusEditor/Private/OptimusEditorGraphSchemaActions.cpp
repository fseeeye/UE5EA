// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusEditorGraphSchemaActions.h"

#include "OptimusEditorGraph.h"
#include "OptimusEditorGraphNode.h"

#include "OptimusResourceDescription.h"
#include "OptimusVariableDescription.h"
#include "OptimusNodeGraph.h"

#include "EdGraph/EdGraphNode.h"


UEdGraphNode* FOptimusGraphSchemaAction_NewNode::PerformAction(
	UEdGraph* InParentGraph, 
	UEdGraphPin* InFromPin, 
	const FVector2D InLocation, 
	bool bInSelectNewNode /*= true*/
	)
{
	check(NodeClass != nullptr);											 

	UOptimusEditorGraph* Graph = Cast<UOptimusEditorGraph>(InParentGraph);
	
	if (ensure(Graph != nullptr))
	{
		UOptimusNode* ModelNode = Graph->GetModelGraph()->AddNode(NodeClass, InLocation);

 		// FIXME: Automatic connection from the given pin.

		UOptimusEditorGraphNode* GraphNode = Graph->FindGraphNodeFromModelNode(ModelNode);
		if (GraphNode && bInSelectNewNode)
		{
			Graph->SelectNodeSet({GraphNode});
		}
		return GraphNode;
	}

	return nullptr;
}

static FText GetGraphSubCategory(UOptimusNodeGraph* InGraph)
{
	if (InGraph->GetGraphType() == EOptimusNodeGraphType::ExternalTrigger)
	{
		return FText::FromString(TEXT("Triggered Graphs"));
	}
	else
	{
		return FText::GetEmpty();
	}
}

static FText GetGraphTooltip(UOptimusNodeGraph* InGraph)
{
	return FText::GetEmpty();
}


FOptimusSchemaAction_Graph::FOptimusSchemaAction_Graph(
	UOptimusNodeGraph* InGraph,
	int32 InGrouping) : 
		FEdGraphSchemaAction(
			GetGraphSubCategory(InGraph), 
			FText::FromString(InGraph->GetName()), 
			GetGraphTooltip(InGraph), 
			InGrouping, 
			FText(), 
			int32(EOptimusSchemaItemGroup::Graphs) 
		), 
		GraphType(InGraph->GetGraphType())
{
	GraphPath = InGraph->GetGraphPath();
}


FOptimusSchemaAction_Resource::FOptimusSchemaAction_Resource(
	UOptimusResourceDescription* InResource, 
	int32 InGrouping ) :
	FEdGraphSchemaAction(
			FText::GetEmpty(),
			FText::FromString(InResource->GetName()),
			FText::GetEmpty(),
			InGrouping,
			FText(),
			int32(EOptimusSchemaItemGroup::Resources)
		)
{
	ResourceName = InResource->GetFName();
}


FOptimusSchemaAction_Variable::FOptimusSchemaAction_Variable(
	UOptimusVariableDescription* InVariable, 
	int32 InGrouping ) : 
	FEdGraphSchemaAction(
          FText::GetEmpty(),
          FText::FromString(InVariable->GetName()),
          FText::GetEmpty(),
          InGrouping,
          FText(),
          int32(EOptimusSchemaItemGroup::Variables))
{
	VariableName = InVariable->GetFName();
}
