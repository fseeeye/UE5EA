// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Renderer.cpp: Renderer module implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "Stats/Stats.h"
#include "Modules/ModuleManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineDefines.h"
#include "EngineGlobals.h"
#include "RenderingThread.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "RenderTargetPool.h"
#include "PostProcess/SceneRenderTargets.h"
#include "VisualizeTexture.h"
#include "SceneCore.h"
#include "SceneHitProxyRendering.h"
#include "SceneRendering.h"
#include "BasePassRendering.h"
#include "MobileBasePassRendering.h"
#include "TranslucentRendering.h"
#include "RendererModule.h"
#include "GPUBenchmark.h"
#include "SystemSettings.h"
#include "VisualizeTexturePresent.h"
#include "MeshPassProcessor.inl"
#include "DebugViewModeRendering.h"
#include "EditorPrimitivesRendering.h"
#include "VisualizeTexturePresent.h"
#include "ScreenSpaceDenoise.h"
#include "VT/VirtualTextureSystem.h"
#include "PostProcess/TemporalAA.h"
#include "CanvasRender.h"
DEFINE_LOG_CATEGORY(LogRenderer);

IMPLEMENT_MODULE(FRendererModule, Renderer);

#if !IS_MONOLITHIC
	// visual studio cannot find cross dll data for visualizers
	// thus as a workaround for now, copy and paste this into every module
	// where we need to visualize SystemSettings
	FSystemSettings* GSystemSettingsForVisualizers = &GSystemSettings;
#endif

static int32 bFlushRenderTargetsOnWorldCleanup = 1;
FAutoConsoleVariableRef CVarFlushRenderTargetsOnWorldCleanup(TEXT("r.bFlushRenderTargetsOnWorldCleanup"), bFlushRenderTargetsOnWorldCleanup, TEXT(""));



void FRendererModule::StartupModule()
{
	GScreenSpaceDenoiser = IScreenSpaceDenoiser::GetDefaultDenoiser();
	GTemporalUpscaler = ITemporalUpscaler::GetDefaultTemporalUpscaler();

	FVirtualTextureSystem::Initialize();
}

void FRendererModule::ShutdownModule()
{
	FVirtualTextureSystem::Shutdown();

	// Free up the memory of the default denoiser. Responsibility of the plugin to free up theirs.
	delete IScreenSpaceDenoiser::GetDefaultDenoiser();
}

void FRendererModule::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources, bool bWorldChanged)
{
	FSceneInterface* Scene = World->Scene;
	ENQUEUE_RENDER_COMMAND(OnWorldCleanup)(
	[Scene, bWorldChanged](FRHICommandListImmediate& RHICmdList)
	{
		if(bFlushRenderTargetsOnWorldCleanup > 0)
		{
			GRenderTargetPool.FreeUnusedResources();
		}
		if(bWorldChanged && Scene)
		{
			Scene->OnWorldCleanup();
		}
	});

}

void FRendererModule::InitializeSystemTextures(FRHICommandListImmediate& RHICmdList)
{
	GSystemTextures.InitializeTextures(RHICmdList, GMaxRHIFeatureLevel);
}

BEGIN_SHADER_PARAMETER_STRUCT(FDrawTileMeshPassParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	SHADER_PARAMETER_STRUCT_REF(FReflectionCaptureShaderData, ReflectionCapture)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FDebugViewModePassUniformParameters, DebugViewMode)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FTranslucentBasePassUniformParameters, TranslucentBasePass)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FOpaqueBasePassUniformParameters, OpaqueBasePass)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FMobileBasePassUniformParameters, MobileBasePass)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void FRendererModule::DrawTileMesh(FCanvasRenderContext& RenderContext, FMeshPassProcessorRenderState& DrawRenderState, const FSceneView& SceneView, FMeshBatch& Mesh, bool bIsHitTesting, const FHitProxyId& HitProxyId, bool bUse128bitRT)
{
	if (!GUsingNullRHI)
	{
		// Create an FViewInfo so we can initialize its RHI resources
		//@todo - reuse this view for multiple tiles, this is going to be slow for each tile
		FViewInfo& View = *RenderContext.Alloc<FViewInfo>(&SceneView);
		View.ViewRect = View.UnscaledViewRect;

		const auto FeatureLevel = View.GetFeatureLevel();
		const EShadingPath ShadingPath = FSceneInterface::GetShadingPath(FeatureLevel);
		const FSceneViewFamily* ViewFamily = View.Family;
		FScene* Scene = nullptr;

		if (ViewFamily->Scene)
		{
			Scene = ViewFamily->Scene->GetRenderScene();
		}

		Mesh.MaterialRenderProxy->UpdateUniformExpressionCacheIfNeeded(FeatureLevel);
		FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();

		//Apply the minimal forward lighting resources
		extern FForwardLightingViewResources* GetMinimalDummyForwardLightingResources();
		View.ForwardLightingResources = GetMinimalDummyForwardLightingResources();

		FSinglePrimitiveStructured& SinglePrimitiveStructured = GTilePrimitiveBuffer;

		if (Mesh.VertexFactory->GetPrimitiveIdStreamIndex(EVertexInputStreamType::PositionOnly) >= 0)
		{
			FMeshBatchElement& MeshElement = Mesh.Elements[0];

			checkf(Mesh.Elements.Num() == 1, TEXT("Only 1 batch element currently supported by DrawTileMesh"));
			checkf(MeshElement.PrimitiveUniformBuffer == nullptr, TEXT("DrawTileMesh does not currently support an explicit primitive uniform buffer on vertex factories which manually fetch primitive data.  Use PrimitiveUniformBufferResource instead."));

			if (MeshElement.PrimitiveUniformBufferResource)
			{
				checkf(MeshElement.NumInstances == 1, TEXT("DrawTileMesh does not currently support instancing"));
				// Force PrimitiveId to be 0 in the shader
				MeshElement.PrimitiveIdMode = PrimID_ForceZero;
				
				// Set the LightmapID to 0, since that's where our light map data resides for this primitive
				FPrimitiveUniformShaderParameters PrimitiveParams = *(const FPrimitiveUniformShaderParameters*)MeshElement.PrimitiveUniformBufferResource->GetContents();
				PrimitiveParams.LightmapDataIndex = 0;
				PrimitiveParams.LightmapUVIndex = 0;

				// Set up reference to the single-instance 
				PrimitiveParams.InstanceDataOffset = 0;
				PrimitiveParams.NumInstanceDataEntries = 1;


				// Now we just need to fill out the first entry of primitive data in a buffer and bind it
				SinglePrimitiveStructured.PrimitiveSceneData = FPrimitiveSceneShaderData(PrimitiveParams);
				SinglePrimitiveStructured.ShaderPlatform = View.GetShaderPlatform();

				// Also fill out correct single-primitive instance data, derived from the primitive.
				SinglePrimitiveStructured.InstanceSceneData = FInstanceSceneShaderData(
					GetInstanceUniformShaderParameters(PrimitiveParams.LocalToWorld, PrimitiveParams.PreviousLocalToWorld, 
						(PrimitiveParams.LocalObjectBoundsMin + PrimitiveParams.LocalObjectBoundsMax) * 0.5f, 
						PrimitiveParams.LocalObjectBoundsMax - PrimitiveParams.LocalObjectBoundsMin,
						PrimitiveParams.NonUniformScale, PrimitiveParams.InvNonUniformScaleAndDeterminantSign, 
						FVector4(ForceInitToZero),
						FNaniteInfo(),
						0,
						0xFFFFFFFFu,
						0.0f,
						true
					));

				// Set up the parameters for the LightmapSceneData from the given LCI data 
				FPrecomputedLightingUniformParameters LightmapParams;
				GetPrecomputedLightingParameters(FeatureLevel, LightmapParams, Mesh.LCI);
				SinglePrimitiveStructured.LightmapSceneData = FLightmapSceneShaderData(LightmapParams);

				SinglePrimitiveStructured.UploadToGPU();

				View.PrimitiveSceneDataOverrideSRV = SinglePrimitiveStructured.PrimitiveSceneDataBufferSRV;
				View.InstanceSceneDataOverrideSRV  = SinglePrimitiveStructured.InstanceSceneDataBufferSRV;
				View.LightmapSceneDataOverrideSRV = SinglePrimitiveStructured.LightmapSceneDataBufferSRV;
			}
		}

		View.InitRHIResources();

		TUniformBufferRef<FReflectionCaptureShaderData> EmptyReflectionCaptureUniformBuffer;

		{
			FReflectionCaptureShaderData EmptyData;
			EmptyReflectionCaptureUniformBuffer = TUniformBufferRef<FReflectionCaptureShaderData>::CreateUniformBufferImmediate(EmptyData, UniformBuffer_SingleFrame);
		}

		//get the blend mode of the material
		const EBlendMode MaterialBlendMode = Mesh.MaterialRenderProxy->GetIncompleteMaterialWithFallback(FeatureLevel).GetBlendMode();

		FRDGBuilder& GraphBuilder = RenderContext.GraphBuilder;

		const bool bUseVirtualTexturing = UseVirtualTexturing(FeatureLevel);
		if (bUseVirtualTexturing)
		{
			RDG_GPU_STAT_SCOPE(GraphBuilder, VirtualTextureUpdate);
			FVirtualTextureSystem::Get().AllocateResources(GraphBuilder, FeatureLevel);
			FVirtualTextureSystem::Get().CallPendingCallbacks();
			FVirtualTextureSystem::Get().Update(GraphBuilder, FeatureLevel, Scene);
		}

		if (!FRDGSystemTextures::IsValid(GraphBuilder))
		{
			FRDGSystemTextures::Create(GraphBuilder);
		}

		RDG_EVENT_SCOPE(GraphBuilder, "DrawTileMesh");

		auto* PassParameters = GraphBuilder.AllocParameters<FDrawTileMeshPassParameters>();
		PassParameters->RenderTargets[0] = FRenderTargetBinding(RenderContext.GetRenderTarget(), ERenderTargetLoadAction::ELoad);
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->ReflectionCapture = EmptyReflectionCaptureUniformBuffer;

		// handle translucent material blend modes, not relevant in MaterialTexCoordScalesAnalysis since it outputs the scales.
		if (ViewFamily->GetDebugViewShaderMode() == DVSM_OutputMaterialTextureScales)
		{
#if WITH_DEBUG_VIEW_MODES
			// make sure we are doing opaque drawing
			DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
			
			// is this path used on mobile?
			if (ShadingPath == EShadingPath::Deferred)
			{
				PassParameters->DebugViewMode = CreateDebugViewModePassUniformBuffer(GraphBuilder, View, nullptr);

				RenderContext.AddPass(RDG_EVENT_NAME("OutputMaterialTextureScales"), PassParameters,
					[Scene, &View, &Mesh](FRHICommandListImmediate& RHICmdList)
				{
					DrawDynamicMeshPass(View, RHICmdList, [&](FMeshPassDrawListContext* InDrawListContext)
					{
						FDebugViewModeMeshProcessor PassMeshProcessor(
							Scene,
							View.GetFeatureLevel(),
							&View,
							false,
							InDrawListContext);
						const uint64 DefaultBatchElementMask = ~0ull;
						PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, nullptr);
					});
				});
			}
#endif // WITH_DEBUG_VIEW_MODES
		}
		else if (IsTranslucentBlendMode(MaterialBlendMode))
		{
			if (ShadingPath == EShadingPath::Deferred)
			{
				PassParameters->TranslucentBasePass = CreateTranslucentBasePassUniformBuffer(GraphBuilder, View);

				RenderContext.AddPass(RDG_EVENT_NAME("TranslucentDeferred"), PassParameters,
					[Scene, &View, &Mesh, DrawRenderState, bUse128bitRT](FRHICommandListImmediate& RHICmdList)
				{
					DrawDynamicMeshPass(View, RHICmdList, [&](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
					{
						FBasePassMeshProcessor PassMeshProcessor(
							Scene,
							View.GetFeatureLevel(),
							&View,
							DrawRenderState,
							DynamicMeshPassContext,
							bUse128bitRT ? FBasePassMeshProcessor::EFlags::bRequires128bitRT : FBasePassMeshProcessor::EFlags::None,
							ETranslucencyPass::TPT_AllTranslucency);

						const uint64 DefaultBatchElementMask = ~0ull;
						PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, nullptr);
					});
				});
			}
			else // Mobile
			{
				PassParameters->MobileBasePass = CreateMobileBasePassUniformBuffer(GraphBuilder, View, EMobileBasePass::Translucent, EMobileSceneTextureSetupMode::None);

				RenderContext.AddPass(RDG_EVENT_NAME("TranslucentMobile"), PassParameters,
					[Scene, &View, DrawRenderState, &Mesh](FRHICommandListImmediate& RHICmdList)
				{
					DrawDynamicMeshPass(View, RHICmdList, [&](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
					{
						FMobileBasePassMeshProcessor PassMeshProcessor(
							Scene,
							View.GetFeatureLevel(),
							&View,
							DrawRenderState,
							DynamicMeshPassContext,
							FMobileBasePassMeshProcessor::EFlags::None,
							ETranslucencyPass::TPT_AllTranslucency);

						const uint64 DefaultBatchElementMask = ~0ull;
						PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, nullptr);
					});
				});
			}
		}
		// handle opaque materials
		else
		{
			// make sure we are doing opaque drawing
			DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());

			// draw the mesh
			if (bIsHitTesting)
			{
				ensureMsgf(HitProxyId == Mesh.BatchHitProxyId, TEXT("Only Mesh.BatchHitProxyId is used for hit testing."));

#if WITH_EDITOR
				RenderContext.AddPass(RDG_EVENT_NAME("HitTesting"), PassParameters,
					[Scene, &View, DrawRenderState, &Mesh](FRHICommandListImmediate& RHICmdList)
				{
					DrawDynamicMeshPass(View, RHICmdList, [&](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
					{
						FHitProxyMeshProcessor PassMeshProcessor(
							Scene,
							&View,
							false,
							DrawRenderState,
							DynamicMeshPassContext);

						const uint64 DefaultBatchElementMask = ~0ull;
						PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, nullptr);
					});
				});
#endif
			}
			else
			{
				if (ShadingPath == EShadingPath::Deferred)
				{
					PassParameters->OpaqueBasePass = CreateOpaqueBasePassUniformBuffer(GraphBuilder, View);

					RenderContext.AddPass(RDG_EVENT_NAME("OpaqueDeferred"), PassParameters,
						[Scene, &View, DrawRenderState, &Mesh, bUse128bitRT](FRHICommandListImmediate& RHICmdList)
					{
						DrawDynamicMeshPass(View, RHICmdList,
							[&](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
						{
							FBasePassMeshProcessor PassMeshProcessor(
								Scene,
								View.GetFeatureLevel(),
								&View,
								DrawRenderState,
								DynamicMeshPassContext,
								bUse128bitRT ? FBasePassMeshProcessor::EFlags::bRequires128bitRT : FBasePassMeshProcessor::EFlags::None);

							const uint64 DefaultBatchElementMask = ~0ull;
							PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, nullptr);
						});
					});
				}
				else // Mobile
				{
					PassParameters->MobileBasePass = CreateMobileBasePassUniformBuffer(GraphBuilder, View, EMobileBasePass::Opaque, EMobileSceneTextureSetupMode::None);

					RenderContext.AddPass(RDG_EVENT_NAME("OpaqueMobile"), PassParameters,
						[Scene, &View, DrawRenderState, &Mesh](FRHICommandListImmediate& RHICmdList)
					{
						DrawDynamicMeshPass(View, RHICmdList, [&](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
						{
							FMobileBasePassMeshProcessor PassMeshProcessor(
								Scene,
								View.GetFeatureLevel(),
								&View,
								DrawRenderState,
								DynamicMeshPassContext,
								FMobileBasePassMeshProcessor::EFlags::CanReceiveCSM);

							const uint64 DefaultBatchElementMask = ~0ull;
							PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, nullptr);
						});
					});
				}
			}
		}
	}
}

void FRendererModule::DebugLogOnCrash()
{
	GVisualizeTexture.DebugLogOnCrash();
	
	GEngine->Exec(NULL, TEXT("rhi.DumpMemory"), *GLog);

	// execute on main thread
	{
		struct FTest
		{
			void Thread()
			{
				GEngine->Exec(NULL, TEXT("Mem FromReport"), *GLog);
			}
		} Test;

		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.DumpDataAfterCrash"),
			STAT_FSimpleDelegateGraphTask_DumpDataAfterCrash,
			STATGROUP_TaskGraphTasks);

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw(&Test, &FTest::Thread),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DumpDataAfterCrash), nullptr, ENamedThreads::GameThread
		);
	}
}

void FRendererModule::GPUBenchmark(FSynthBenchmarkResults& InOut, float WorkScale)
{
	check(IsInGameThread());

	FSceneViewInitOptions ViewInitOptions;
	FIntRect ViewRect(0, 0, 1, 1);

	FBox LevelBox(FVector(-WORLD_MAX), FVector(+WORLD_MAX));
	ViewInitOptions.SetViewRectangle(ViewRect);

	// Initialize Projection Matrix and ViewMatrix since FSceneView initialization is doing some math on them.
	// Otherwise it trips NaN checks.
	const FVector ViewPoint = LevelBox.GetCenter();
	ViewInitOptions.ViewOrigin = FVector(ViewPoint.X, ViewPoint.Y, 0.0f);
	ViewInitOptions.ViewRotationMatrix = FMatrix(
		FPlane(1, 0, 0, 0),
		FPlane(0, -1, 0, 0),
		FPlane(0, 0, -1, 0),
		FPlane(0, 0, 0, 1));

	const float ZOffset = WORLD_MAX;
	ViewInitOptions.ProjectionMatrix = FReversedZOrthoMatrix(
		LevelBox.GetSize().X / 2.f,
		LevelBox.GetSize().Y / 2.f,
		0.5f / ZOffset,
		ZOffset
		);

	FSceneView DummyView(ViewInitOptions);
	FlushRenderingCommands();
	FSynthBenchmarkResults* InOutPtr = &InOut;
	ENQUEUE_RENDER_COMMAND(RendererGPUBenchmarkCommand)(
		[DummyView, WorkScale, InOutPtr](FRHICommandListImmediate& RHICmdList)
		{
			RendererGPUBenchmark(RHICmdList, *InOutPtr, DummyView, WorkScale);
		});
	FlushRenderingCommands();
}

static void VisualizeTextureExec( const TCHAR* Cmd, FOutputDevice &Ar )
{
	check(IsInGameThread());
	FlushRenderingCommands();
	GVisualizeTexture.ParseCommands(Cmd, Ar);
}

extern void NaniteStatsFilterExec(const TCHAR* Cmd, FOutputDevice& Ar);
extern void ResetRenderTargetsExtent(FOutputDevice& Ar);

static bool RendererExec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
#if SUPPORTS_VISUALIZE_TEXTURE
	if (FParse::Command(&Cmd, TEXT("VisualizeTexture")) || FParse::Command(&Cmd, TEXT("Vis")))
	{
		VisualizeTextureExec(Cmd, Ar);
		return true;
	}
#endif //SUPPORTS_VISUALIZE_TEXTURE
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (FParse::Command(&Cmd, TEXT("DumpUnbuiltLightInteractions")))
	{
		InWorld->Scene->DumpUnbuiltLightInteractions(Ar);
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("NaniteStats")))
	{
		NaniteStatsFilterExec(Cmd, Ar);
		return true;
	}
	else if(FParse::Command(&Cmd, TEXT("r.RHI.Name")))
	{
		Ar.Logf( TEXT( "Running on the %s RHI" ), GDynamicRHI 
			? (GDynamicRHI->GetName() ? GDynamicRHI->GetName() : TEXT("<NULL Name>"))
			: TEXT("<NULL DynamicRHI>"));
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("r.ResetRenderTargetsExtent")))
	{
		ResetRenderTargetsExtent(Ar);
		return true;
	}
#endif

	return false;
}

ICustomCulling* GCustomCullingImpl = nullptr;

void FRendererModule::RegisterCustomCullingImpl(ICustomCulling* impl)
{
	check(GCustomCullingImpl == nullptr);
	GCustomCullingImpl = impl;
}

void FRendererModule::UnregisterCustomCullingImpl(ICustomCulling* impl)
{
	check(GCustomCullingImpl == impl);
	GCustomCullingImpl = nullptr;
}

FStaticSelfRegisteringExec RendererExecRegistration(RendererExec);

void FRendererModule::ExecVisualizeTextureCmd( const FString& Cmd )
{
	// @todo: Find a nicer way to call this
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	VisualizeTextureExec(*Cmd, *GLog);
#endif
}
