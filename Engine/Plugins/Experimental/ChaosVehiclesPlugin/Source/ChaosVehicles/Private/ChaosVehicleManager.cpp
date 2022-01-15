// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVehicleManager.h"
#include "UObject/UObjectIterator.h"

#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Physics/PhysicsFiltering.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "ChaosVehicleMovementComponent.h"

#include "PBDRigidsSolver.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

DECLARE_CYCLE_STAT(TEXT("VehicleManager:ParallelUpdateVehicles"), STAT_ChaosVehicleManager_ParallelUpdateVehicles, STATGROUP_ChaosVehicleManager);
DECLARE_CYCLE_STAT(TEXT("VehicleManager:Update"), STAT_ChaosVehicleManager_Update, STATGROUP_ChaosVehicleManager);
DECLARE_CYCLE_STAT(TEXT("VehicleManager:ScenePreTick"), STAT_ChaosVehicleManager_ScenePreTick, STATGROUP_ChaosVehicleManager);


extern FVehicleDebugParams GVehicleDebugParams;

TMap<FPhysScene*, FChaosVehicleManager*> FChaosVehicleManager::SceneToVehicleManagerMap;
uint32 FChaosVehicleManager::VehicleSetupTag = 0;

FDelegateHandle FChaosVehicleManager::OnPostWorldInitializationHandle;
FDelegateHandle FChaosVehicleManager::OnWorldCleanupHandle;

#if WITH_CHAOS
bool FChaosVehicleManager::GInitialized = false;
#endif

FChaosVehicleManager::FChaosVehicleManager(FPhysScene* PhysScene)
#if WITH_CHAOS
	: Scene(*PhysScene)
	, AsyncCallback(nullptr)
	, Timestamp(0)

#endif
{
	check(PhysScene);
	
#if WITH_CHAOS
	if (!GInitialized)
	{
		GInitialized = true;
		// PhysScene->GetOwningWorld() is always null here, the world is being setup too late to be of use
		// therefore setup these global world delegates that will callback when everything is setup so registering
		// the physics solver Async Callback will succeed
		OnPostWorldInitializationHandle = FWorldDelegates::OnPostWorldInitialization.AddStatic(&FChaosVehicleManager::OnPostWorldInitialization);
		OnWorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&FChaosVehicleManager::OnWorldCleanup);
	}

	// Add to Scene-To-Manager map
	FChaosVehicleManager::SceneToVehicleManagerMap.Add(PhysScene, this);
#endif
}


void FChaosVehicleManager::RegisterCallbacks()
{
#if WITH_CHAOS
	OnPhysScenePreTickHandle = Scene.OnPhysScenePreTick.AddRaw(this, &FChaosVehicleManager::Update);
	OnPhysSceneStepHandle = Scene.OnPhysSceneStep.AddRaw(this, &FChaosVehicleManager::SubStep);
	OnPhysScenePostTickHandle = Scene.OnPhysScenePostTick.AddRaw(this, &FChaosVehicleManager::PostUpdate);

	check(AsyncCallback == nullptr);
	AsyncCallback = Scene.GetSolver()->CreateAndRegisterSimCallbackObject_External<FChaosVehicleManagerAsyncCallback>(true);
#endif
}

void FChaosVehicleManager::UnregisterCallbacks()
{
#if WITH_CHAOS
	Scene.OnPhysScenePreTick.Remove(OnPhysScenePreTickHandle);
	Scene.OnPhysSceneStep.Remove(OnPhysSceneStepHandle);
	Scene.OnPhysScenePostTick.Remove(OnPhysScenePostTickHandle);

	if (AsyncCallback)
	{
		Scene.GetSolver()->UnregisterAndFreeSimCallbackObject_External(AsyncCallback);
		AsyncCallback = nullptr;
	}
#endif
}

void FChaosVehicleManager::OnPostWorldInitialization(UWorld* InWorld, const UWorld::InitializationValues)
{
	FChaosVehicleManager* Manager = FChaosVehicleManager::GetVehicleManagerFromScene(InWorld->GetPhysicsScene());
	if (Manager)
	{
		Manager->RegisterCallbacks();
	}
}


void FChaosVehicleManager::OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
{
	FChaosVehicleManager* Manager = FChaosVehicleManager::GetVehicleManagerFromScene(InWorld->GetPhysicsScene());
	if (Manager)
	{
		Manager->UnregisterCallbacks();
	}
}

void FChaosVehicleManager::DetachFromPhysScene(FPhysScene* PhysScene)
{
	if (AsyncCallback)
	{
		UnregisterCallbacks();
	}

	if (PhysScene->GetOwningWorld())
	{
		PhysScene->GetOwningWorld()->OnWorldBeginPlay.RemoveAll(this);
	}

	FChaosVehicleManager::SceneToVehicleManagerMap.Remove(PhysScene);
}

FChaosVehicleManager::~FChaosVehicleManager()
{
	while (Vehicles.Num() > 0)
	{
		RemoveVehicle(Vehicles.Last());
	}
}

FChaosVehicleManager* FChaosVehicleManager::GetVehicleManagerFromScene(FPhysScene* PhysScene)
{
	FChaosVehicleManager* Manager = nullptr;
	FChaosVehicleManager** ManagerPtr = SceneToVehicleManagerMap.Find(PhysScene);
	if (ManagerPtr != nullptr)
	{
		Manager = *ManagerPtr;
	}
	return Manager;
}

void FChaosVehicleManager::AddVehicle(TWeakObjectPtr<UChaosVehicleMovementComponent> Vehicle)
{
	check(Vehicle != NULL);
	check(Vehicle->PhysicsVehicleOutput());
	check(AsyncCallback);

	Vehicles.Add(Vehicle);
}

void FChaosVehicleManager::RemoveVehicle(TWeakObjectPtr<UChaosVehicleMovementComponent> Vehicle)
{
	check(Vehicle != NULL);
	check(Vehicle->PhysicsVehicleOutput());

	Vehicles.Remove(Vehicle);

	if (Vehicle->PhysicsVehicleOutput().IsValid())
	{
		Vehicle->PhysicsVehicleOutput().Reset(nullptr);
	}

}

void FChaosVehicleManager::ScenePreTick(FPhysScene* PhysScene, float DeltaTime)
{
	// inputs being set via back door, i.e. accessing PVehicle directly is a no go now, needs to go through async input system
	SCOPE_CYCLE_COUNTER(STAT_ChaosVehicleManager_ScenePreTick);

	for (int32 i = 0; i < Vehicles.Num(); ++i)
	{
		Vehicles[i]->PreTickGT(DeltaTime);
	}

}

void FChaosVehicleManager::Update(FPhysScene* PhysScene, float DeltaTime)
{
#if WITH_CHAOS
	SCOPE_CYCLE_COUNTER(STAT_ChaosVehicleManager_Update);

	UWorld* World = Scene.GetOwningWorld();

	SubStepCount = 0;

	ScenePreTick(PhysScene, DeltaTime);

	ParallelUpdateVehicles(DeltaTime);

	if (World)
	{
		FChaosVehicleManagerAsyncInput* AsyncInput = AsyncCallback->GetProducerInputData_External();
		for (TWeakObjectPtr<UChaosVehicleMovementComponent> Vehicle : Vehicles)
		{
			Vehicle->Update(DeltaTime);
			Vehicle->FinalizeSimCallbackData(*AsyncInput);
		}
	}
#endif
}

void FChaosVehicleManager::SubStep(FPhysScene* PhysScene, float DeltaTime)
{
// #TODO: needs a callback from the physics thread during sub-step
	//if (SubStepCount++ > 0)	//ignore first substep
	//{
	//	ParallelUpdateVehicles(DeltaTime);
	//	//if (UWorld* World = GetWorld())
	//	//{
	//	//	for (TWeakObjectPtr<UChaosVehicleMovementComponent> Vehicle : Vehicles)
	//	//	{
	//	//		if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Vehicle->GetRootComponent()))
	//	//		{
	//	//			if (RootPrim->GetBodyInstance() && !Vehicle->IsAsleep())
	//	//			{
	//	//				Vehicle->Substep(DeltaTime);
	//	//			}
	//	//		}
	//	//	}
	//	//}
	//}
}

void FChaosVehicleManager::PostUpdate(FChaosScene* PhysScene)
{
	// Unused
}

void FChaosVehicleManager::ParallelUpdateVehicles(float DeltaSeconds)
{
#if WITH_CHAOS
	SCOPE_CYCLE_COUNTER(STAT_ChaosVehicleManager_ParallelUpdateVehicles);

	FChaosVehicleManagerAsyncInput* AsyncInput = AsyncCallback->GetProducerInputData_External();

	AsyncInput->Reset();	//only want latest frame's data

	{
		// We pass pointers from TArray so this reserve is critical. Otherwise realloc happens
		AsyncInput->VehicleInputs.Reserve(Vehicles.Num());
		AsyncInput->Timestamp = Timestamp;
		AsyncInput->World = Scene.GetOwningWorld();
	}

	// Grab all outputs for processing, even future ones for interpolation.
	{
		Chaos::TSimCallbackOutputHandle<FChaosVehicleManagerAsyncOutput> AsyncOutputLatest;
		while ((AsyncOutputLatest = AsyncCallback->PopFutureOutputData_External()))
		{
			PendingOutputs.Emplace(MoveTemp(AsyncOutputLatest));
		}
	}

	// Since we are in pre-physics, delta seconds is not accounted for in external time yet
	const float ResultsTime = AsyncCallback->GetSolver()->GetPhysicsResultsTime_External() + DeltaSeconds;

	// Find index of first non-consumable output (first one after current time)
	int32 LastOutputIdx = 0;
	for (; LastOutputIdx < PendingOutputs.Num(); ++LastOutputIdx)
	{
		if (PendingOutputs[LastOutputIdx]->InternalTime > ResultsTime)
		{
			break;
		}
	}

	// Process events on all outputs which occurred before current time
	// 
	//for (int32 OutputIdx = 0; OutputIdx < LastOutputIdx; ++OutputIdx)
	//{
	//	for (TWeakObjectPtr<UChaosVehicleMovementComponent> Vehicle : Vehicles)
	//	{
	//		Vehicle->GameThread_ProcessIntermediateAsyncOutput(*PendingOutputs[OutputIdx]);
	//	}
	//}

	// Cache the last consumed output for interpolation
	if (LastOutputIdx > 0)
	{
		LatestOutput = MoveTemp(PendingOutputs[LastOutputIdx - 1]);
	}

	// Remove all consumed outputs
	{
		TArray<Chaos::TSimCallbackOutputHandle<FChaosVehicleManagerAsyncOutput>> NewPendingOutputs;
		for (int32 OutputIdx = LastOutputIdx; OutputIdx < PendingOutputs.Num(); ++OutputIdx)
		{
			NewPendingOutputs.Emplace(MoveTemp(PendingOutputs[OutputIdx]));
		}
		PendingOutputs = MoveTemp(NewPendingOutputs);
	}

	// It's possible we will end up multiple frames ahead of output, take the latest ready output.
	Chaos::TSimCallbackOutputHandle<FChaosVehicleManagerAsyncOutput> AsyncOutput;
	Chaos::TSimCallbackOutputHandle<FChaosVehicleManagerAsyncOutput> AsyncOutputLatest;
	while ((AsyncOutputLatest = AsyncCallback->PopOutputData_External()))
	{
		AsyncOutput = MoveTemp(AsyncOutputLatest);

		// Note: not used - left as a reminder
		//for (TWeakObjectPtr<UChaosVehicleMovementComponent> Vehicle : Vehicles)
		//{
		//	//Vehicle->GameThread_ProcessIntermediateAsyncOutput(*AsyncOutput);
		//}
	}

	if (UWorld* World = Scene.GetOwningWorld())
	{
		int32 NumVehiclesInActiveBatch = 0;
		for (TWeakObjectPtr<UChaosVehicleMovementComponent> Vehicle : Vehicles)
		{
			auto NextOutput = PendingOutputs.Num() > 0 ? PendingOutputs[0].Get() : nullptr;
			float Alpha = 0.f;
			if (NextOutput && LatestOutput)
			{
				const float Denom = NextOutput->InternalTime - LatestOutput->InternalTime;
				if (Denom > SMALL_NUMBER)
				{
					Alpha = (ResultsTime - LatestOutput->InternalTime) / Denom;
				}
			}

			AsyncInput->VehicleInputs.Add(Vehicle->SetCurrentAsyncInputOutput(AsyncInput->VehicleInputs.Num(), LatestOutput.Get(), NextOutput, Alpha, Timestamp));
		}
	}

	++Timestamp;

	const auto& AwakeVehiclesBatch = Vehicles; // TODO: process awake only

	auto LambdaParallelUpdate = [DeltaSeconds, &AwakeVehiclesBatch](int32 Idx)
	{
		TWeakObjectPtr<UChaosVehicleMovementComponent> Vehicle = AwakeVehiclesBatch[Idx];
		Vehicle->ParallelUpdate(DeltaSeconds); // gets output state from PT
	};

	bool ForceSingleThread = !GVehicleDebugParams.EnableMultithreading;
	ParallelFor(AwakeVehiclesBatch.Num(), LambdaParallelUpdate, ForceSingleThread);
#endif
}