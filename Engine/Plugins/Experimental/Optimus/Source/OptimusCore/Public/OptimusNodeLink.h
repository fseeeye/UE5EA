// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "OptimusNodeLink.generated.h"

class UOptimusNodePin;

UCLASS()
class OPTIMUSCORE_API UOptimusNodeLink : public UObject
{
	GENERATED_BODY()

public:
	UOptimusNodeLink() = default;

	/// Returns the output pin on the node this link connects from.
	UOptimusNodePin* GetNodeOutputPin() const { return NodeOutputPin; }

	/// Returns the input pin on the node that this link connects to.
	UOptimusNodePin* GetNodeInputPin() const { return NodeInputPin; }

protected:
	friend class UOptimusNodeGraph;

	UPROPERTY()
	UOptimusNodePin* NodeOutputPin;

	UPROPERTY()
	UOptimusNodePin* NodeInputPin;
};
