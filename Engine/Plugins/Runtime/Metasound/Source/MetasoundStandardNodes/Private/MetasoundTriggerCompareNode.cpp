// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundTriggerCompareNode.h"
#include "MetasoundStandardNodesCategories.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes"

namespace Metasound
{
	namespace MetasoundTriggerCompareNodePrivate
	{
		FNodeClassMetadata CreateNodeClassMetadata(const FName& InDataTypeName, const FName& InOperatorName, const FText& InDisplayName, const FText& InDescription, const FVertexInterface& InDefaultInterface)
		{
			FNodeClassMetadata Metadata
			{
				FNodeClassName{FName("TriggerCompare"), InOperatorName, InDataTypeName},
				1, // Major Version
				0, // Minor Version
				InDisplayName,
				InDescription,
				PluginAuthor,
				PluginNodeMissingPrompt,
				InDefaultInterface,
				{StandardNodes::TriggerUtils},
				{TEXT("TriggerCompare")},
				FNodeDisplayStyle{}
			};

			return Metadata;
		}
	}

	using FTriggerCompareNodeInt32 = TTriggerCompareNode<int32>;
	METASOUND_REGISTER_NODE(FTriggerCompareNodeInt32)

	using FTriggerCompareNodeFloat = TTriggerCompareNode<float>;
	METASOUND_REGISTER_NODE(FTriggerCompareNodeFloat)
}

#undef LOCTEXT_NAMESPACE // MetasoundTriggerDelayNode
