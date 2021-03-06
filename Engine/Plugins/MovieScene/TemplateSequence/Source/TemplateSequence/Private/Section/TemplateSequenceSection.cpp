// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/TemplateSequenceSection.h"
#include "TemplateSequence.h"
#include "Systems/TemplateSequenceSystem.h"
#include "Evaluation/MovieSceneRootOverridePath.h"
#include "EntitySystem/MovieSceneSequenceInstance.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"

UTemplateSequenceSection::UTemplateSequenceSection(const FObjectInitializer& ObjInitializer)
	: Super(ObjInitializer)
{
	SetBlendType(EMovieSceneBlendType::Absolute);

	// Template sequences always adopt the same hierarchical bias as their parent sequence so that their
	// animation can blend with any complementary animation set directly on their target object.
	Parameters.HierarchicalBias = 0;
}

void UTemplateSequenceSection::OnDilated(float DilationFactor, FFrameNumber Origin)
{
	// TODO-lchabant: shouldn't this be in the base class?
	Parameters.TimeScale /= DilationFactor;
}

void UTemplateSequenceSection::ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity)
{
	using namespace UE::MovieScene;

	FTemplateSequenceComponentData ComponentData;

	if (UTemplateSequence* TemplateSubSequence = Cast<UTemplateSequence>(GetSequence()))
	{
		const FSubSequencePath      PathToRoot = EntityLinker->GetInstanceRegistry()->GetInstance(Params.Sequence.InstanceHandle).GetSubSequencePath();
		const FMovieSceneSequenceID ResolvedSequenceID = PathToRoot.ResolveChildSequenceID(this->GetSequenceID());

		ComponentData.InnerOperand = FMovieSceneEvaluationOperand(ResolvedSequenceID, TemplateSubSequence->GetRootObjectBindingID());
	}

	FGuid ObjectBindingID = Params.GetObjectBindingID();

	OutImportedEntity->AddBuilder(
		FEntityBuilder()
		.AddConditional(FBuiltInComponentTypes::Get()->GenericObjectBinding, ObjectBindingID, ObjectBindingID.IsValid())
		.Add(FTemplateSequenceComponentTypes::Get()->TemplateSequence, ComponentData)
	);

	BuildDefaultSubSectionComponents(EntityLinker, Params, OutImportedEntity);
}
