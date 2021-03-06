// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Simulation/RigUnit_Verlet.h"
#include "Units/RigUnitContext.h"

FRigUnit_VerletIntegrateVector_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		bInitialized = false;
		return;
	}

	if (!bInitialized)
	{
		Point.Mass = 1.f;
		Position = Point.Position = Target;
		Velocity = Acceleration = Point.LinearVelocity = FVector::ZeroVector;
		bInitialized = true;
		return;
	}

	Point.LinearDamping = Damp;
	if (Context.DeltaTime > SMALL_NUMBER)
	{
		float U = FMath::Clamp<float>(Blend * Context.DeltaTime, 0.f, 1.f);
		FVector Force = (Target - Point.Position) * FMath::Max(Strength, 0.0001f);
		FVector PreviousVelocity = Point.LinearVelocity;
		Point = Point.IntegrateVerlet(Force, Blend, Context.DeltaTime);
		Acceleration = (Point.LinearVelocity - PreviousVelocity) / Context.DeltaTime;
		Position = Point.Position;
		Velocity = Point.LinearVelocity;
	}
}
