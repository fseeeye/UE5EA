// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_RandomPlayer.h"

#include "EditorCategoryUtils.h"
#include "Animation/AnimAttributes.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_RandomPlayer"

FLinearColor UAnimGraphNode_RandomPlayer::GetNodeTitleColor() const
{
	return FLinearColor(0.10f, 0.60f, 0.12f);
}

FText UAnimGraphNode_RandomPlayer::GetTooltipText() const
{
	return LOCTEXT("NodeToolTip", "Plays sequences picked from a provided list in random orders.");
}

FText UAnimGraphNode_RandomPlayer::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Random Sequence Player");
}

FText UAnimGraphNode_RandomPlayer::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Animation);
}

void UAnimGraphNode_RandomPlayer::GetOutputLinkAttributes(FNodeAttributeArray& OutAttributes) const
{
	OutAttributes.Add(UE::Anim::FAttributes::Curves);
	OutAttributes.Add(UE::Anim::FAttributes::Attributes);
}

#undef LOCTEXT_NAMESPACE
