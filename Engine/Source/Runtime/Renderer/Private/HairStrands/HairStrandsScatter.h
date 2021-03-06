// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"
#include "Renderer/Private/SceneRendering.h"

void AddHairDiffusionPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const struct FHairStrandsVisibilityData& VisibilityData,
	const struct FHairStrandsVoxelResources& VoxelResources,
	const FRDGTextureRef SceneColorDepth,
	FRDGTextureRef OutSceneColorTexture);