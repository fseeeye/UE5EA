// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"
#include "MeshBatch.h"

#if RHI_RAYTRACING
#include "RayTracingDefinitions.h"

struct FRayTracingInstance
{
	/** The underlying geometry of this instance specification. */
	const FRayTracingGeometry* Geometry;

	/**
	 * Materials for each segment, in the form of mesh batches. We will check whether every segment of the geometry has been assigned a material.
	 * Unlike the raster path, mesh batches assigned here are considered transient and will be discarded immediately upon we finished gathering for the current scene proxy.
	 */
	TArray<FMeshBatch> Materials;

	/** Whether the instance is forced opaque, i.e. anyhit shaders are disabled on this instance */
	bool bForceOpaque : 1;

	/** Instance mask that can be used to exclude the instance from specific effects (eg. ray traced shadows). */
	uint8 Mask = RAY_TRACING_MASK_ALL;

	/** 
	* Transforms count. When NumTransforms == 1 we create a single instance. 
	* When it's more than one we create multiple identical instances with different transforms. 
	* When GPU transforms are used it is a conservative count. NumTransforms should be less or equal to `InstanceTransforms.Num() 
	*/
	uint32 NumTransforms = 0;

	/** Instance transforms. */
	TArray<FMatrix> InstanceTransforms;

	/** Similar to InstanceTransforms, but memory is owned by someone else (i.g. FPrimitiveSceneProxy). */
	TArrayView<FMatrix> InstanceTransformsView;

	/** When instance transforms are only available in GPU, this SRV holds them. */
	FShaderResourceViewRHIRef InstanceGPUTransformsSRV;

	/** Build mask and flags based on materials specified in Materials. You can still override Mask after calling this function. */
	ENGINE_API void BuildInstanceMaskAndFlags();
};

ENGINE_API uint8 ComputeBlendModeMask(const EBlendMode BlendMode);
#endif
