// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimeTrace.h"

#if ANIM_TRACE_ENABLED

#include "Trace/Trace.inl"
#include "Animation/BlendSpaceBase.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_BlendSpaceGraphBase.h"
#include "Animation/AnimInstanceProxy.h"

UE_TRACE_EVENT_BEGIN(Animation, BlendSpacePlayer)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, AnimInstanceId)
	UE_TRACE_EVENT_FIELD(uint64, BlendSpaceId)
	UE_TRACE_EVENT_FIELD(int32, NodeId)
	UE_TRACE_EVENT_FIELD(float, PositionX)
	UE_TRACE_EVENT_FIELD(float, PositionY)
	UE_TRACE_EVENT_FIELD(float, PositionZ)
	UE_TRACE_EVENT_FIELD(float, FilteredPositionX)
	UE_TRACE_EVENT_FIELD(float, FilteredPositionY)
	UE_TRACE_EVENT_FIELD(float, FilteredPositionZ)
	UE_TRACE_EVENT_END()

void FAnimGraphRuntimeTrace::OutputBlendSpacePlayer(const FAnimationBaseContext& InContext, const FAnimNode_BlendSpacePlayer& InNode)
{
	bool bEventEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(AnimationChannel);
	if (!bEventEnabled)
	{
		return;
	}

	check(InContext.AnimInstanceProxy);

	TRACE_OBJECT(InContext.AnimInstanceProxy->GetAnimInstanceObject());
	TRACE_OBJECT(InNode.BlendSpace);

	UE_TRACE_LOG(Animation, BlendSpacePlayer, AnimationChannel)
		<< BlendSpacePlayer.Cycle(FPlatformTime::Cycles64())
		<< BlendSpacePlayer.AnimInstanceId(FObjectTrace::GetObjectId(InContext.AnimInstanceProxy->GetAnimInstanceObject()))
		<< BlendSpacePlayer.BlendSpaceId(FObjectTrace::GetObjectId(InNode.BlendSpace))
		<< BlendSpacePlayer.NodeId(InContext.GetCurrentNodeId())
		<< BlendSpacePlayer.PositionX(InNode.X)
		<< BlendSpacePlayer.PositionY(InNode.Y)
		<< BlendSpacePlayer.PositionZ(InNode.Z)
		<< BlendSpacePlayer.FilteredPositionX(InNode.GetFilteredPosition().X)
		<< BlendSpacePlayer.FilteredPositionY(InNode.GetFilteredPosition().Y)
		<< BlendSpacePlayer.FilteredPositionZ(InNode.GetFilteredPosition().Z);
}

void FAnimGraphRuntimeTrace::OutputBlendSpace(const FAnimationBaseContext& InContext, const FAnimNode_BlendSpaceGraphBase& InNode)
{
	bool bEventEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(AnimationChannel);
	if (!bEventEnabled)
	{
		return;
	}

	check(InContext.AnimInstanceProxy);

	TRACE_OBJECT(InContext.AnimInstanceProxy->GetAnimInstanceObject());
	TRACE_OBJECT(InNode.GetBlendSpace());

	FVector Coordinates = InNode.GetPosition();
	FVector FilteredCoordinates = InNode.GetFilteredPosition();

	UE_TRACE_LOG(Animation, BlendSpacePlayer, AnimationChannel)
		<< BlendSpacePlayer.Cycle(FPlatformTime::Cycles64())
		<< BlendSpacePlayer.AnimInstanceId(FObjectTrace::GetObjectId(InContext.AnimInstanceProxy->GetAnimInstanceObject()))
		<< BlendSpacePlayer.BlendSpaceId(FObjectTrace::GetObjectId(InNode.GetBlendSpace()))
		<< BlendSpacePlayer.NodeId(InContext.GetCurrentNodeId())
		<< BlendSpacePlayer.PositionX(Coordinates.X)
		<< BlendSpacePlayer.PositionY(Coordinates.Y)
		<< BlendSpacePlayer.PositionZ(Coordinates.Z)
		<< BlendSpacePlayer.FilteredPositionX(FilteredCoordinates.X)
		<< BlendSpacePlayer.FilteredPositionY(FilteredCoordinates.Y)
		<< BlendSpacePlayer.FilteredPositionZ(FilteredCoordinates.Z);
}

#endif