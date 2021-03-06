// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryFlowCoreNodes.h"
#include "GeometryFlowImmutableData.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"

#include "Image/ImageBuilder.h"

namespace UE
{
namespace GeometryFlow
{


struct FTextureImage
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::TextureImage);

	TImageBuilder<FVector4f> Image;
};

// declares FDataTextureImage, FTextureImageInput, FTextureImageOutput, FTextureImageSourceNode
GEOMETRYFLOW_DECLARE_BASIC_TYPES(TextureImage, FTextureImage, (int)EMeshProcessingDataTypes::TextureImage)



}	// end namespace GeometryFlow
}	// end namespace UE