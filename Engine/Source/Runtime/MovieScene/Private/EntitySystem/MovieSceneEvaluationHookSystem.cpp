// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneEvaluationHookSystem.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "EntitySystem/MovieSceneSpawnablesSystem.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"

#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "IMovieScenePlayer.h"

DECLARE_CYCLE_STAT(TEXT("Generic Hooks"),  MovieSceneECS_GenericHooks, STATGROUP_MovieSceneECS);

namespace UE
{
namespace MovieScene
{

struct FEvaluationHookUpdater
{
	UMovieSceneEvaluationHookSystem* HookSystem;
	FInstanceRegistry* InstanceRegistry;

	FEvaluationHookUpdater(UMovieSceneEvaluationHookSystem* InHookSystem, FInstanceRegistry* InInstanceRegistry)
		: HookSystem(InHookSystem), InstanceRegistry(InInstanceRegistry)
	{}

	void ForEachEntity(FInstanceHandle InstanceHandle, const FMovieSceneEvaluationHookComponent& Hook, FFrameTime EvalTime, FEvaluationHookFlags& InOutFlags)
	{
		if (InOutFlags.bHasBegun == false)
		{
			InOutFlags.bHasBegun = true;
			return;
		}

		const FSequenceInstance&  SequenceInstance = InstanceRegistry->GetInstance(InstanceHandle);

		FMovieSceneEvaluationHookEvent NewEvent;
		NewEvent.Hook       = Hook;
		NewEvent.Type       = EEvaluationHookEvent::Update;
		NewEvent.RootTime   = EvalTime * SequenceInstance.GetContext().GetSequenceToRootTransform();
		NewEvent.SequenceID = SequenceInstance.GetSequenceID();

		HookSystem->AddEvent(SequenceInstance.GetRootInstanceHandle(), NewEvent);
	}
};

struct FEvaluationHookSorter
{
	UMovieSceneEvaluationHookSystem* HookSystem;

	FEvaluationHookSorter(UMovieSceneEvaluationHookSystem* InHookSystem)
		: HookSystem(InHookSystem)
	{}

	FORCEINLINE TStatId           GetStatId() const    { return GET_STATID(MovieSceneECS_GenericHooks); }
	static ENamedThreads::Type    GetDesiredThread()   { return ENamedThreads::AnyHiPriThreadHiPriTask; }
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Run();
	}

	void Run()
	{
		HookSystem->SortEvents();
	}
};

} // namespace MovieScene
} // namespace UE

UMovieSceneEvaluationHookSystem::UMovieSceneEvaluationHookSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	Phase = UE::MovieScene::ESystemPhase::Instantiation | UE::MovieScene::ESystemPhase::Evaluation | UE::MovieScene::ESystemPhase::Finalization;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineComponentConsumer(GetClass(), UE::MovieScene::FBuiltInComponentTypes::Get()->EvalTime);
	}
}

void UMovieSceneEvaluationHookSystem::AddEvent(UE::MovieScene::FInstanceHandle RootInstance, const FMovieSceneEvaluationHookEvent& InEvent)
{
	PendingEventsByRootInstance.FindOrAdd(RootInstance).Events.Add(InEvent);
}

bool UMovieSceneEvaluationHookSystem::HasEvents() const
{
	return PendingEventsByRootInstance.Num() != 0;
}

bool UMovieSceneEvaluationHookSystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	return HasEvents() || InLinker->EntityManager.ContainsComponent(UE::MovieScene::FBuiltInComponentTypes::Get()->EvaluationHook);
}

void UMovieSceneEvaluationHookSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FMovieSceneEntitySystemRunner* Runner = Linker->GetActiveRunner();
	if (!ensure(Runner))
	{
		return;
	}

	ESystemPhase CurrentPhase = Runner->GetCurrentPhase();
	if (CurrentPhase == ESystemPhase::Instantiation)
	{
		UpdateHooks();
	}
	else if (CurrentPhase == ESystemPhase::Evaluation)
	{
		FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();

		FGraphEventRef UpdateEvent = FEntityTaskBuilder()
		.Read(Components->InstanceHandle)
		.Read(Components->EvaluationHook)
		.Read(Components->EvalTime)
		.Write(Components->EvaluationHookFlags)
		.Dispatch_PerEntity<FEvaluationHookUpdater>(&Linker->EntityManager, InPrerequisites, &Subsequents, this, Linker->GetInstanceRegistry());

		if (Linker->EntityManager.GetThreadingModel() == EEntityThreadingModel::NoThreading)
		{
			this->SortEvents();
		}
		else
		{
			// The only thing we depend on is the gather task
			FGraphEventArray Prereqs = { UpdateEvent };
			FGraphEventRef SortTask = TGraphTask<FEvaluationHookSorter>::CreateTask(&Prereqs, Linker->EntityManager.GetDispatchThread())
			.ConstructAndDispatchWhenReady(this);

			Subsequents.AddMasterTask(SortTask);
		}
	}
	else if (HasEvents())
	{
		ensure(CurrentPhase == ESystemPhase::Finalization);
		Runner->GetQueuedEventTriggers().AddUObject(this, &UMovieSceneEvaluationHookSystem::TriggerAllEvents);
	}
}

void UMovieSceneEvaluationHookSystem::UpdateHooks()
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();

	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	auto VisitNew = [this, InstanceRegistry](FInstanceHandle InstanceHandle, FFrameTime EvalTime, const FMovieSceneEvaluationHookComponent& Hook)
	{
		const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(InstanceHandle);

		FMovieSceneEvaluationHookEvent NewEvent;
		NewEvent.Hook       = Hook;
		NewEvent.Type       = EEvaluationHookEvent::Begin;
		NewEvent.RootTime   = EvalTime * SequenceInstance.GetContext().GetSequenceToRootTransform();
		NewEvent.SequenceID = SequenceInstance.GetSequenceID();

		this->AddEvent(SequenceInstance.GetRootInstanceHandle(), NewEvent);
	};

	auto VisitOld = [this, InstanceRegistry](FInstanceHandle InstanceHandle, FFrameTime EvalTime, const FMovieSceneEvaluationHookComponent& Hook)
	{
		const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(InstanceHandle);

		FMovieSceneEvaluationHookEvent NewEvent;
		NewEvent.Hook       = Hook;
		NewEvent.Type       = EEvaluationHookEvent::End;
		NewEvent.RootTime   = EvalTime * SequenceInstance.GetContext().GetSequenceToRootTransform();
		NewEvent.SequenceID = SequenceInstance.GetSequenceID();

		this->AddEvent(SequenceInstance.GetRootInstanceHandle(), NewEvent);
	};

	FEntityTaskBuilder()
	.Read(Components->InstanceHandle)
	.Read(Components->EvalTime)
	.Read(Components->EvaluationHook)
	.FilterAny({ Components->Tags.NeedsLink })
	.Iterate_PerEntity(&Linker->EntityManager, VisitNew);

	FEntityTaskBuilder()
	.Read(Components->InstanceHandle)
	.Read(Components->EvalTime)
	.Read(Components->EvaluationHook)
	.FilterAny({ Components->Tags.Finished })
	.Iterate_PerEntity(&Linker->EntityManager, VisitOld);
}

void UMovieSceneEvaluationHookSystem::SortEvents()
{
	using namespace UE::MovieScene;

	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	for (TPair<FMovieSceneEvaluationInstanceKey, FMovieSceneEvaluationHookEventContainer>& Pair : PendingEventsByRootInstance)
	{
		const FSequenceInstance& RootInstance = InstanceRegistry->GetInstance(Pair.Key.InstanceHandle);
		if (RootInstance.GetContext().GetDirection() == EPlayDirection::Forwards)
		{
			Algo::SortBy(Pair.Value.Events, &FMovieSceneEvaluationHookEvent::RootTime);
		}
		else
		{
			Algo::SortBy(Pair.Value.Events, &FMovieSceneEvaluationHookEvent::RootTime, TGreater<>());
		}
	}
}

void UMovieSceneEvaluationHookSystem::TriggerAllEvents()
{
	using namespace UE::MovieScene;

	SCOPE_CYCLE_COUNTER(MovieSceneECS_GenericHooks);

	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	// We need to clean our state before actually triggering the events because one of those events could
	// call back into an evaluation (for instance, by starting play on another sequence). If we don't clean
	// this before, would would re-enter and re-trigger past events, resulting in an infinite loop!
	TMap<FMovieSceneEvaluationInstanceKey, FMovieSceneEvaluationHookEventContainer> LocalEvents;
	Swap(LocalEvents, PendingEventsByRootInstance);

	for (TPair<FMovieSceneEvaluationInstanceKey, FMovieSceneEvaluationHookEventContainer>& Pair : LocalEvents)
	{
		const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(Pair.Key.InstanceHandle);

		IMovieScenePlayer* Player      = SequenceInstance.GetPlayer();
		FMovieSceneContext RootContext = SequenceInstance.GetContext();

		for (const FMovieSceneEvaluationHookEvent& Event : Pair.Value.Events)
		{
			FEvaluationHookParams Params = {
				Event.Hook.ObjectBindingID, RootContext, Event.SequenceID, Event.TriggerIndex
			};

			if (Event.SequenceID != MovieSceneSequenceID::Root)
			{
				FInstanceHandle SubInstance = SequenceInstance.FindSubInstance(Event.SequenceID);
				if (SubInstance.IsValid())
				{
					Params.Context = InstanceRegistry->GetInstance(SubInstance).GetContext();
				}
			}

			switch (Event.Type)
			{
				case EEvaluationHookEvent::Begin:
					Event.Hook.Interface->Begin(Player, Params);
					break;
				case EEvaluationHookEvent::Update:
					Event.Hook.Interface->Update(Player, Params);
					break;
				case EEvaluationHookEvent::End:
					Event.Hook.Interface->End(Player, Params);
					break;

				case EEvaluationHookEvent::Trigger:
					Event.Hook.Interface->Trigger(Player, Params);
					break;
			}
		}
	}
}