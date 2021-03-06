// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Buffer.cpp: D3D Common code for buffers.
=============================================================================*/

#include "D3D12RHIPrivate.h"

struct FRHICommandUpdateBufferString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandUpdateBuffer"); }
};
struct FRHICommandUpdateBuffer final : public FRHICommand<FRHICommandUpdateBuffer, FRHICommandUpdateBufferString>
{
	FD3D12ResourceLocation Source;
	FD3D12ResourceLocation* Destination;
	uint32 NumBytes;
	uint32 DestinationOffset;

	FORCEINLINE_DEBUGGABLE FRHICommandUpdateBuffer(FD3D12ResourceLocation* InDest, FD3D12ResourceLocation& InSource, uint32 InDestinationOffset, uint32 InNumBytes)
		: Source(nullptr)
		, Destination(InDest)
		, NumBytes(InNumBytes)
		, DestinationOffset(InDestinationOffset)
	{
		FD3D12ResourceLocation::TransferOwnership(Source, InSource);
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		FD3D12DynamicRHI::GetD3DRHI()->UpdateBuffer(Destination->GetResource(), Destination->GetOffsetFromBaseOfResource() + DestinationOffset, Source.GetResource(), Source.GetOffsetFromBaseOfResource(), NumBytes);
	}
};

// This allows us to rename resources from the RenderThread i.e. all the 'hard' work of allocating a new resource
// is done in parallel and this small function is called to switch the resource to point to the correct location
// a the correct time.
struct FRHICommandRenameUploadBufferString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandRenameUploadBuffer"); }
};
struct FRHICommandRenameUploadBuffer final : public FRHICommand<FRHICommandRenameUploadBuffer, FRHICommandRenameUploadBufferString>
{
	FD3D12Buffer* Resource;
	FD3D12ResourceLocation NewLocation;

	FORCEINLINE_DEBUGGABLE FRHICommandRenameUploadBuffer(FD3D12Buffer* InResource, FD3D12Device* Device)
		: Resource(InResource)
		, NewLocation(Device) 
	{}

	void Execute(FRHICommandListBase& CmdList)
	{
		// Clear the resource if still bound to make sure the SRVs are rebound again on next operation
		FD3D12CommandContext& Context = (FD3D12CommandContext&)(CmdList.IsImmediateAsyncCompute() ? CmdList.GetComputeContext().GetLowestLevelContext() : CmdList.GetContext().GetLowestLevelContext());
		Context.ConditionalClearShaderResource(&Resource->ResourceLocation);

		Resource->RenameLDAChain(NewLocation);
	}
};

struct FD3D12RHICommandInitializeBufferString
{
	static const TCHAR* TStr() { return TEXT("FD3D12RHICommandInitializeBuffer"); }
};
struct FD3D12RHICommandInitializeBuffer final : public FRHICommand<FD3D12RHICommandInitializeBuffer, FD3D12RHICommandInitializeBufferString>
{
	FD3D12Buffer* Buffer;
	FD3D12ResourceLocation SrcResourceLoc;
	uint32 Size;
	D3D12_RESOURCE_STATES DestinationState;

	FORCEINLINE_DEBUGGABLE FD3D12RHICommandInitializeBuffer(FD3D12Buffer* InBuffer, FD3D12ResourceLocation& InSrcResourceLoc, uint32 InSize, D3D12_RESOURCE_STATES InDestinationState)
		: Buffer(InBuffer)
		, SrcResourceLoc(InSrcResourceLoc.GetParentDevice())
		, Size(InSize)
		, DestinationState(InDestinationState)
	{
		FD3D12ResourceLocation::TransferOwnership(SrcResourceLoc, InSrcResourceLoc);
	}

	void Execute(FRHICommandListBase& /* unused */)
	{
		ExecuteNoCmdList();
	}

	void ExecuteNoCmdList()
	{
		for (FD3D12Buffer::FLinkedObjectIterator CurrentBuffer(Buffer); CurrentBuffer; ++CurrentBuffer)
		{
			FD3D12Resource* Destination = CurrentBuffer->ResourceLocation.GetResource();
			FD3D12Device* Device = Destination->GetParentDevice();

			FD3D12CommandContext& CommandContext = Device->GetDefaultCommandContext();
			FD3D12CommandListHandle& hCommandList = CommandContext.CommandListHandle;
			// Copy from the temporary upload heap to the default resource
			{
				// if resource doesn't require state tracking then transition to copy dest here (could have been suballocated from shared resource) - not very optimal and should be batched
				if (!Destination->RequiresResourceStateTracking())
				{
					hCommandList.AddTransitionBarrier(Destination, Destination->GetDefaultResourceState(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
				}

				CommandContext.numInitialResourceCopies++;
				hCommandList.FlushResourceBarriers();
				hCommandList->CopyBufferRegion(
					Destination->GetResource(),
					CurrentBuffer->ResourceLocation.GetOffsetFromBaseOfResource(),
					SrcResourceLoc.GetResource()->GetResource(),
					SrcResourceLoc.GetOffsetFromBaseOfResource(), Size);

				// Update the resource state after the copy has been done (will take care of updating the residency as well)
				if (DestinationState != D3D12_RESOURCE_STATE_COPY_DEST)
				{
					hCommandList.AddTransitionBarrier(Destination, D3D12_RESOURCE_STATE_COPY_DEST, DestinationState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
				}

				if (Destination->RequiresResourceStateTracking())
				{
					// Update the tracked resource state of this resource in the command list
					CResourceState& ResourceState = hCommandList.GetResourceState(Destination);
					ResourceState.SetResourceState(DestinationState);
					Destination->GetResourceState().SetResourceState(DestinationState);

					// Add dummy pending barrier, because the end state needs to be updated after execture command list with tracked state in the command list
					hCommandList.AddPendingResourceBarrier(Destination, D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
				}
				else
				{
					check(Destination->GetDefaultResourceState() == DestinationState);
				}

				hCommandList.UpdateResidency(SrcResourceLoc.GetResource());

				CommandContext.ConditionalFlushCommandList();
			}

			// Buffer is now written and ready, so unlock the block (locked after creation and can be defragmented if needed)
			CurrentBuffer->ResourceLocation.UnlockPoolData();
		}
	}
};

void FD3D12Adapter::AllocateBuffer(FD3D12Device* Device,
	const D3D12_RESOURCE_DESC& InDesc,
	uint32 Size,
	uint32 InUsage,
	ED3D12ResourceStateMode InResourceStateMode,
	D3D12_RESOURCE_STATES InCreateState,
	FRHIResourceCreateInfo& CreateInfo,
	uint32 Alignment,
	FD3D12Buffer* Buffer,
	FD3D12ResourceLocation& ResourceLocation,
	ID3D12ResourceAllocator* ResourceAllocator)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(D3D12RHI::AllocateBuffer);

	// Explicitly check that the size is nonzero before allowing CreateBuffer to opaquely fail.
	check(Size > 0);

	const bool bIsDynamic = (InUsage & BUF_AnyDynamic) ? true : false;

	if (bIsDynamic)
	{
		check(ResourceAllocator == nullptr);
		check(InResourceStateMode != ED3D12ResourceStateMode::MultiState);
		check(InCreateState == D3D12_RESOURCE_STATE_GENERIC_READ);
		void* pData = GetUploadHeapAllocator(Device->GetGPUIndex()).AllocUploadResource(Size, Alignment, ResourceLocation);
		check(ResourceLocation.GetSize() == Size);

		if (CreateInfo.ResourceArray)
		{
			const void* InitialData = CreateInfo.ResourceArray->GetResourceData();

			check(Size == CreateInfo.ResourceArray->GetResourceDataSize());
			// Handle initial data
			FMemory::Memcpy(pData, InitialData, Size);
		}
	}
	else
	{
		if (ResourceAllocator)
		{
			ResourceAllocator->AllocateResource(Device->GetGPUIndex(), D3D12_HEAP_TYPE_DEFAULT, InDesc, InDesc.Width, Alignment, InResourceStateMode, InCreateState, nullptr, CreateInfo.DebugName, ResourceLocation);
		}
		else
		{
			Device->GetDefaultBufferAllocator().AllocDefaultResource(D3D12_HEAP_TYPE_DEFAULT, InDesc, (EBufferUsageFlags)InUsage, InResourceStateMode, InCreateState, ResourceLocation, Alignment, CreateInfo.DebugName);
		}
		ResourceLocation.SetOwner(Buffer);
		check(ResourceLocation.GetSize() == Size);
	}
}

FD3D12Buffer* FD3D12Adapter::CreateRHIBuffer(FRHICommandListImmediate* RHICmdList,
	const D3D12_RESOURCE_DESC& InDesc,
	uint32 Alignment,
	uint32 Stride,
	uint32 Size,
	uint32 InUsage,
	ED3D12ResourceStateMode InResourceStateMode,
	ERHIAccess InResourceState,
	FRHIResourceCreateInfo& CreateInfo,
	ID3D12ResourceAllocator* ResourceAllocator)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(D3D12RHI::CreateRHIBuffer);

	SCOPE_CYCLE_COUNTER(STAT_D3D12CreateBufferTime);

	check(InDesc.Width == Size);

	const bool bIsDynamic = (InUsage & BUF_AnyDynamic) ? true : false;
	const uint32 FirstGPUIndex = CreateInfo.GPUMask.GetFirstIndex();

	// Transient flag set?
	bool bIsTransient = (InUsage & BUF_Transient);
	
	// Does this resource support tracking?
	const bool bSupportResourceStateTracking = !bIsDynamic && FD3D12DefaultBufferAllocator::IsPlacedResource(InDesc.Flags, InResourceStateMode);

	// Initial state is derived from the InResourceState if it's supports tracking
	D3D12_HEAP_TYPE HeapType = bIsDynamic ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
	const FD3D12Resource::FD3D12ResourceTypeHelper Type(InDesc, HeapType);
	const D3D12_RESOURCE_STATES InitialState = bSupportResourceStateTracking ? Type.GetOptimalInitialState(InResourceState, false) : 
		FD3D12DefaultBufferAllocator::GetDefaultInitialResourceState(HeapType, (EBufferUsageFlags)InUsage, InResourceStateMode);

	FD3D12Buffer* BufferOut = nullptr;
	if (bIsDynamic)
	{
		// Assume not transient and dynamic
		check(!bIsTransient);
		
		FD3D12Buffer* NewBuffer0 = nullptr;
		BufferOut = CreateLinkedObject<FD3D12Buffer>(CreateInfo.GPUMask, [&](FD3D12Device* Device)
		{
			FD3D12Buffer* NewBuffer = new FD3D12Buffer(Device, Size, InUsage, Stride);
			NewBuffer->BufferAlignment = Alignment;

			if (Device->GetGPUIndex() == FirstGPUIndex)
			{
				AllocateBuffer(Device, InDesc, Size, InUsage, InResourceStateMode, InitialState, CreateInfo, Alignment, NewBuffer, NewBuffer->ResourceLocation, ResourceAllocator);
				NewBuffer0 = NewBuffer;
			}
			else
			{
				check(NewBuffer0);
				FD3D12ResourceLocation::ReferenceNode(Device, NewBuffer->ResourceLocation, NewBuffer0->ResourceLocation);
			}

			return NewBuffer;
		});
	}
	else
	{
		// Setup the state at which the resource needs to be created - copy dest only supported for placed resources
		const D3D12_RESOURCE_STATES CreateState = (CreateInfo.ResourceArray && bSupportResourceStateTracking) ? D3D12_RESOURCE_STATE_COPY_DEST : InitialState;

		BufferOut = CreateLinkedObject<FD3D12Buffer>(CreateInfo.GPUMask, [&](FD3D12Device* Device)
		{
			FD3D12Buffer* NewBuffer = new FD3D12Buffer(Device, Size, InUsage, Stride);
			NewBuffer->BufferAlignment = Alignment;
			AllocateBuffer(Device, InDesc, Size, InUsage, InResourceStateMode, CreateState, CreateInfo, Alignment, NewBuffer, NewBuffer->ResourceLocation, ResourceAllocator);
			NewBuffer->ResourceLocation.SetTransient(bIsTransient);

			// Unlock immediately if no initial data
			if (CreateInfo.ResourceArray == nullptr)
			{
				NewBuffer->ResourceLocation.UnlockPoolData();
			}

			return NewBuffer;
		});
	}

	if (CreateInfo.ResourceArray)
	{
		check(!bIsTransient);
		if (bIsDynamic == false && BufferOut->ResourceLocation.IsValid())
		{
			check(Size == CreateInfo.ResourceArray->GetResourceDataSize());

			const bool bOnAsyncThread = !IsInRHIThread() && !IsInRenderingThread();

			// Get an upload heap and initialize data
			FD3D12ResourceLocation SrcResourceLoc(BufferOut->GetParentDevice());
			void* pData;
			if (bOnAsyncThread)
			{
				const uint32 GPUIdx = SrcResourceLoc.GetParentDevice()->GetGPUIndex();
				pData = GetUploadHeapAllocator(GPUIdx).AllocUploadResource(Size, 4u, SrcResourceLoc);
			}
			else
			{
				pData = SrcResourceLoc.GetParentDevice()->GetDefaultFastAllocator().Allocate(Size, 4UL, &SrcResourceLoc);
			}
			check(pData);
			FMemory::Memcpy(pData, CreateInfo.ResourceArray->GetResourceData(), Size);
			
			if (bOnAsyncThread)
			{
				// Need to update buffer content on RHI thread (immediate context) because the buffer can be a
				// sub-allocation and its backing resource may be in a state incompatible with the copy queue.
				// TODO:
				// Create static buffers in COMMON state, rely on state promotion/decay to avoid transition barriers,
				// and initialize them asynchronously on the copy queue. D3D12 buffers always allow simultaneous acess
				// so it is legal to write to a region on the copy queue while other non-overlapping regions are
				// being read on the graphics/compute queue. Currently, d3ddebug throws error for such usage.
				// Once Microsoft (via Windows update) fix the debug layer, async static buffer initialization should
				// be done on the copy queue.
				FD3D12ResourceLocation* SrcResourceLoc_Heap = new FD3D12ResourceLocation(SrcResourceLoc.GetParentDevice());
				FD3D12ResourceLocation::TransferOwnership(*SrcResourceLoc_Heap, SrcResourceLoc);
				ENQUEUE_RENDER_COMMAND(CmdD3D12InitializeBuffer)(
					[BufferOut, SrcResourceLoc_Heap, Size, InitialState](FRHICommandListImmediate& RHICmdList)
				{
					if (RHICmdList.Bypass())
					{
						FD3D12RHICommandInitializeBuffer Command(BufferOut, *SrcResourceLoc_Heap, Size, InitialState);
						Command.ExecuteNoCmdList();
					}
					else
					{
						new (RHICmdList.AllocCommand<FD3D12RHICommandInitializeBuffer>()) FD3D12RHICommandInitializeBuffer(BufferOut, *SrcResourceLoc_Heap, Size, InitialState);
					}
					delete SrcResourceLoc_Heap;
				});
			}
			else if (!RHICmdList || RHICmdList->Bypass())
			{
				// On RHIT or RT (when bypassing), we can access immediate context directly
				FD3D12RHICommandInitializeBuffer Command(BufferOut, SrcResourceLoc, Size, InitialState);
				Command.ExecuteNoCmdList();
			}
			else
			{
				// On RT but not bypassing
				new (RHICmdList->AllocCommand<FD3D12RHICommandInitializeBuffer>()) FD3D12RHICommandInitializeBuffer(BufferOut, SrcResourceLoc, Size, InitialState);
			}
		}

		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	// Don't update stats for transient resources
	if (!bIsTransient)
	{
		UpdateBufferStats(GetBufferStats(InUsage), BufferOut->ResourceLocation.GetSize());
	}

	return BufferOut;
}

void FD3D12Buffer::Rename(FD3D12ResourceLocation& NewLocation)
{
	FD3D12ResourceLocation::TransferOwnership(ResourceLocation, NewLocation);
	ResourceRenamed(&ResourceLocation);
}

void FD3D12Buffer::RenameLDAChain(FD3D12ResourceLocation& NewLocation)
{
	// Dynamic buffers use cross-node resources.
	//ensure(GetUsage() & BUF_AnyDynamic);
	Rename(NewLocation);

	if (GNumExplicitGPUsForRendering > 1)
	{
		// This currently crashes at exit time because NewLocation isn't tracked in the right allocator.
		ensure(IsHeadLink());
		ensure(GetParentDevice() == NewLocation.GetParentDevice());

		// Update all of the resources in the LDA chain to reference this cross-node resource
		for (auto NextBuffer = ++FLinkedObjectIterator(this); NextBuffer; ++NextBuffer)
		{
			FD3D12ResourceLocation::ReferenceNode(NextBuffer->GetParentDevice(), NextBuffer->ResourceLocation, ResourceLocation);
			NextBuffer->ResourceRenamed(&NextBuffer->ResourceLocation);
		}
	}
}

void FD3D12Buffer::Swap(FD3D12Buffer& Other)
{
	check(!LockedData.bLocked && !Other.LockedData.bLocked);
	FRHIBuffer::Swap(Other);
	FD3D12BaseShaderResource::Swap(Other);
	FD3D12TransientResource::Swap(Other);
	FD3D12LinkedAdapterObject<FD3D12Buffer>::Swap(Other);
}

void FD3D12Buffer::ReleaseUnderlyingResource()
{
	check(IsHeadLink());
	for (FLinkedObjectIterator NextBuffer(this); NextBuffer; ++NextBuffer)
	{
		check(!NextBuffer->LockedData.bLocked && NextBuffer->ResourceLocation.IsValid());
		NextBuffer->ResourceLocation.Clear();
		NextBuffer->RemoveAllRenameListeners();
	}
}

void FD3D12Buffer::GetResourceDescAndAlignment(uint64 InSize, uint32 InStride, EBufferUsageFlags& InUsage, D3D12_RESOURCE_DESC& ResourceDesc, uint32& Alignment)
{
	ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(InSize);

	if (InUsage & BUF_UnorderedAccess)
	{
		ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		static bool bRequiresRawView = (GMaxRHIFeatureLevel < ERHIFeatureLevel::SM5);
		if (bRequiresRawView)
		{
			// Force the buffer to be a raw, byte address buffer
			InUsage |= BUF_ByteAddressBuffer;
		}
	}

	if ((InUsage & BUF_ShaderResource) == 0)
	{
		ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	if (InUsage & BUF_DrawIndirect)
	{
		ResourceDesc.Flags |= D3D12RHI_RESOURCE_FLAG_ALLOW_INDIRECT_BUFFER;
	}

	// Structured buffers, non-ByteAddress buffers, need to be aligned to their stride to ensure that they can be addressed correctly with element based offsets.
	Alignment = (InStride > 0) && (((InUsage & BUF_StructuredBuffer) != 0) || ((InUsage & (BUF_ByteAddressBuffer | BUF_DrawIndirect)) == 0)) ? InStride : 4;

}

FBufferRHIRef FD3D12DynamicRHI::RHICreateBuffer(uint32 Size, EBufferUsageFlags Usage, uint32 Stride, ERHIAccess InResourceState, FRHIResourceCreateInfo& CreateInfo)
{	
	return CreateBuffer(nullptr, Size, Usage, Stride, InResourceState, CreateInfo);
}

FBufferRHIRef FD3D12DynamicRHI::CreateBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, EBufferUsageFlags Usage, uint32 Stride, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo)
{
	return CreateBuffer(&RHICmdList, Size, Usage, Stride, ResourceState, CreateInfo);
}

FBufferRHIRef FD3D12DynamicRHI::CreateBuffer(FRHICommandListImmediate* RHICmdList, uint32 Size, EBufferUsageFlags Usage, uint32 Stride, ERHIAccess InResourceState, FRHIResourceCreateInfo& CreateInfo)
{
	if (CreateInfo.bWithoutNativeResource)
	{
		return GetAdapter().CreateLinkedObject<FD3D12Buffer>(CreateInfo.GPUMask, [](FD3D12Device* Device)
			{
				return new FD3D12Buffer();
			});
	}

	ID3D12ResourceAllocator* ResourceAllocator = nullptr;
	return CreateD3D12Buffer(RHICmdList, Size, Usage, Stride, InResourceState, CreateInfo, ResourceAllocator);
}

FD3D12Buffer* FD3D12DynamicRHI::CreateD3D12Buffer(class FRHICommandListImmediate* RHICmdList, uint32 Size, EBufferUsageFlags Usage, uint32 Stride, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo, ID3D12ResourceAllocator* ResourceAllocator)
{
	D3D12_RESOURCE_DESC Desc;
	uint32 Alignment;
	FD3D12Buffer::GetResourceDescAndAlignment(Size, Stride, Usage, Desc, Alignment);

	FD3D12Buffer* Buffer = GetAdapter().CreateRHIBuffer(RHICmdList, Desc, Alignment, Stride, Size, Usage, ED3D12ResourceStateMode::Default, ResourceState, CreateInfo, ResourceAllocator);
	if (Buffer->ResourceLocation.IsTransient())
	{
		// TODO: this should ideally be set in platform-independent code, since this tracking is for the high level
		Buffer->SetCommitted(false);
	}

	return Buffer;
}

FRHIBuffer* FD3D12DynamicRHI::CreateBuffer(const FRHIBufferCreateInfo& CreateInfo, const TCHAR* DebugName, ERHIAccess InitialState, ID3D12ResourceAllocator* ResourceAllocator)
{
	FRHIResourceCreateInfo ResourceCreateInfo(DebugName);
	return CreateD3D12Buffer(nullptr, CreateInfo.Size, CreateInfo.Usage, CreateInfo.Stride, InitialState, ResourceCreateInfo, ResourceAllocator);
}

void* FD3D12DynamicRHI::LockBuffer(FRHICommandListImmediate* RHICmdList, FD3D12Buffer* Buffer, uint32 BufferSize, uint32 BufferUsage, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12LockBufferTime);

	check(Size <= BufferSize);

	FD3D12LockedResource& LockedData = Buffer->LockedData;
	check(LockedData.bLocked == false);
	FD3D12Adapter& Adapter = GetAdapter();

	// Determine whether the buffer is dynamic or not.
	const bool bIsDynamic = (BufferUsage & BUF_AnyDynamic) ? true : false;

	void* Data = nullptr;

	if (bIsDynamic)
	{
		check(LockMode == RLM_WriteOnly || LockMode == RLM_WriteOnly_NoOverwrite);

		if (LockedData.bHasNeverBeenLocked)
		{
			// Buffers on upload heap are mapped right after creation
			Data = Buffer->ResourceLocation.GetMappedBaseAddress();
			check(!!Data);
		}
		else
		{
			FD3D12Device* Device = Buffer->GetParentDevice();

			// If on the RenderThread, queue up a command on the RHIThread to rename this buffer at the correct time
			if (ShouldDeferBufferLockOperation(RHICmdList) && LockMode == RLM_WriteOnly)
			{
				FRHICommandRenameUploadBuffer* Command = ALLOC_COMMAND_CL(*RHICmdList, FRHICommandRenameUploadBuffer)(Buffer, Device);

				Data = Adapter.GetUploadHeapAllocator(Device->GetGPUIndex()).AllocUploadResource(BufferSize, Buffer->BufferAlignment, Command->NewLocation);
				RHICmdList->RHIThreadFence(true);
			}
			else
			{
				FRHICommandRenameUploadBuffer Command(Buffer, Device);
				Data = Adapter.GetUploadHeapAllocator(Device->GetGPUIndex()).AllocUploadResource(BufferSize, Buffer->BufferAlignment, Command.NewLocation);
				Command.Execute(*RHICmdList);
			}
		}
	}
	else
	{
		// Static and read only buffers only have one version of the content. Use the first related device.
		FD3D12Device* Device = Buffer->GetParentDevice();
		FD3D12Resource* pResource = Buffer->ResourceLocation.GetResource();

		// Locking for read must occur immediately so we can't queue up the operations later.
		if (LockMode == RLM_ReadOnly)
		{
			LockedData.bLockedForReadOnly = true;
			// If the static buffer is being locked for reading, create a staging buffer.
			FD3D12Resource* StagingBuffer = nullptr;

			const FRHIGPUMask Node = Device->GetGPUMask();
			VERIFYD3D12RESULT(Adapter.CreateBuffer(D3D12_HEAP_TYPE_READBACK, Node, Node, Offset + Size, &StagingBuffer, nullptr));

			// Copy the contents of the buffer to the staging buffer.
			{
				const auto& pfnCopyContents = [&]()
				{
					FD3D12CommandContext& DefaultContext = Device->GetDefaultCommandContext();

					FD3D12CommandListHandle& hCommandList = DefaultContext.CommandListHandle;
					FScopedResourceBarrier ScopeResourceBarrierSource(hCommandList, pResource, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, FD3D12DynamicRHI::ETransitionMode::Apply);
					// Don't need to transition upload heaps

					uint64 SubAllocOffset = Buffer->ResourceLocation.GetOffsetFromBaseOfResource();

					DefaultContext.numCopies++;
					hCommandList.FlushResourceBarriers();	// Must flush so the desired state is actually set.
					hCommandList->CopyBufferRegion(
						StagingBuffer->GetResource(),
						0,
						pResource->GetResource(),
						SubAllocOffset + Offset, Size);

					hCommandList.UpdateResidency(StagingBuffer);
					hCommandList.UpdateResidency(pResource);

					DefaultContext.FlushCommands(true);
				};

				if (ShouldDeferBufferLockOperation(RHICmdList))
				{
					// Sync when in the render thread implementation
					check(IsInRHIThread() == false);

					RHICmdList->ImmediateFlush(EImmediateFlushType::FlushRHIThread);
					pfnCopyContents();
				}
				else
				{
					check(IsInRenderingThread() && !IsRHIThreadRunning());
					pfnCopyContents();
				}
			}

			LockedData.ResourceLocation.AsStandAlone(StagingBuffer, Size);
			Data = LockedData.ResourceLocation.GetMappedBaseAddress();
		}
		else
		{
			// If the static buffer is being locked for writing, allocate memory for the contents to be written to.
			Data = Device->GetDefaultFastAllocator().Allocate(Size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, &LockedData.ResourceLocation);
		}
	}

	LockedData.LockedOffset = Offset;
	LockedData.LockedPitch = Size;
	LockedData.bLocked = true;
	LockedData.bHasNeverBeenLocked = false;

	// Return the offset pointer
	check(Data != nullptr);
	return Data;
}

void FD3D12DynamicRHI::UnlockBuffer(FRHICommandListImmediate* RHICmdList, FD3D12Buffer* Buffer, uint32 BufferUsage)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12UnlockBufferTime);

	FD3D12LockedResource& LockedData = Buffer->LockedData;
	check(LockedData.bLocked == true);

	// Determine whether the buffer is dynamic or not.
	const bool bIsDynamic = (BufferUsage & BUF_AnyDynamic) != 0;

	if (bIsDynamic)
	{
		// If the Buffer is dynamic, its upload heap memory can always stay mapped. Don't do anything.
	}
	else
	{
		if (LockedData.bLockedForReadOnly)
		{
			//Nothing to do, just release the locked data at the end of the function
		}
		else
		{
			// Update all of the resources in the LDA chain
			check(Buffer->IsHeadLink());
			for (FD3D12Buffer::FLinkedObjectIterator CurrentBuffer(Buffer); CurrentBuffer; ++CurrentBuffer)
			{
				// If we are on the render thread, queue up the copy on the RHIThread so it happens at the correct time.
				if (ShouldDeferBufferLockOperation(RHICmdList))
				{
					if (GNumExplicitGPUsForRendering == 1)
					{
						ALLOC_COMMAND_CL(*RHICmdList, FRHICommandUpdateBuffer)(&CurrentBuffer->ResourceLocation, LockedData.ResourceLocation, LockedData.LockedOffset, LockedData.LockedPitch);
					}
					else // The resource location must be copied because the constructor in FRHICommandUpdateBuffer transfers ownership and clears it.
					{
						FD3D12ResourceLocation NodeResourceLocation(LockedData.ResourceLocation.GetParentDevice());
						FD3D12ResourceLocation::ReferenceNode(NodeResourceLocation.GetParentDevice(), NodeResourceLocation, LockedData.ResourceLocation);
						ALLOC_COMMAND_CL(*RHICmdList, FRHICommandUpdateBuffer)(&CurrentBuffer->ResourceLocation, NodeResourceLocation, LockedData.LockedOffset, LockedData.LockedPitch);
					}
				}
				else
				{
					UpdateBuffer(CurrentBuffer->ResourceLocation.GetResource(),
						CurrentBuffer->ResourceLocation.GetOffsetFromBaseOfResource() + LockedData.LockedOffset,
						LockedData.ResourceLocation.GetResource(),
						LockedData.ResourceLocation.GetOffsetFromBaseOfResource(),
						LockedData.LockedPitch);
				}
			}
		}
	}

	LockedData.Reset();
}

void* FD3D12DynamicRHI::RHILockBuffer(FRHICommandListImmediate& RHICmdList, FRHIBuffer* BufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FD3D12Buffer* Buffer = FD3D12DynamicRHI::ResourceCast(BufferRHI);
	return LockBuffer(&RHICmdList, Buffer, Buffer->GetSize(), Buffer->GetUsage(), Offset, Size, LockMode);
}

void FD3D12DynamicRHI::RHIUnlockBuffer(FRHICommandListImmediate& RHICmdList, FRHIBuffer* BufferRHI)
{
	FD3D12Buffer* Buffer = FD3D12DynamicRHI::ResourceCast(BufferRHI);
	UnlockBuffer(&RHICmdList, Buffer, Buffer->GetUsage());
}

void FD3D12DynamicRHI::RHITransferBufferUnderlyingResource(FRHIBuffer* DestBuffer, FRHIBuffer* SrcBuffer)
{
	check(DestBuffer);
	FD3D12Buffer* Dest = ResourceCast(DestBuffer);
	if (!SrcBuffer)
	{
		Dest->ReleaseUnderlyingResource();
	}
	else
	{
		FD3D12Buffer* Src = ResourceCast(SrcBuffer);
		Dest->Swap(*Src);
	}
}

void FD3D12DynamicRHI::RHICopyBuffer(FRHIBuffer* SourceBufferRHI, FRHIBuffer* DestBufferRHI)
{
	FD3D12Buffer* SrcBuffer = FD3D12DynamicRHI::ResourceCast(SourceBufferRHI);
	FD3D12Buffer* DstBuffer = FD3D12DynamicRHI::ResourceCast(DestBufferRHI);
	check(SrcBuffer->GetSize() == DstBuffer->GetSize());

	FD3D12Buffer* SourceBuffer = SrcBuffer;
	FD3D12Buffer* DestBuffer = DstBuffer;

	for (FD3D12Buffer::FDualLinkedObjectIterator It(SourceBuffer, DestBuffer); It; ++It)
	{
		SourceBuffer = It.GetFirst();
		DestBuffer = It.GetSecond();

		FD3D12Device* Device = SourceBuffer->GetParentDevice();
		check(Device == DestBuffer->GetParentDevice());

		FD3D12Resource* pSourceResource = SourceBuffer->ResourceLocation.GetResource();
		D3D12_RESOURCE_DESC const& SourceBufferDesc = pSourceResource->GetDesc();

		FD3D12Resource* pDestResource = DestBuffer->ResourceLocation.GetResource();
		D3D12_RESOURCE_DESC const& DestBufferDesc = pDestResource->GetDesc();

		check(SourceBufferDesc.Width == DestBufferDesc.Width);

		FD3D12CommandContext& Context = Device->GetDefaultCommandContext();
		Context.numCopies++;
		Context.CommandListHandle->CopyResource(pDestResource->GetResource(), pSourceResource->GetResource());
		Context.CommandListHandle.UpdateResidency(pDestResource);
		Context.CommandListHandle.UpdateResidency(pSourceResource);

		DEBUG_EXECUTE_COMMAND_CONTEXT(Device->GetDefaultCommandContext());

		Device->RegisterGPUWork(1);
	}
}

#if D3D12_RHI_RAYTRACING
void FD3D12CommandContext::RHICopyBufferRegion(FRHIBuffer* DestBufferRHI, uint64 DstOffset, FRHIBuffer* SourceBufferRHI, uint64 SrcOffset, uint64 NumBytes)
{
	FD3D12Buffer* SourceBuffer = RetrieveObject<FD3D12Buffer>(SourceBufferRHI);
	FD3D12Buffer* DestBuffer = RetrieveObject<FD3D12Buffer>(DestBufferRHI);

	FD3D12Device* Device = SourceBuffer->GetParentDevice();
	check(Device == DestBuffer->GetParentDevice());
	check(Device == GetParentDevice());

	FD3D12Resource* pSourceResource = SourceBuffer->ResourceLocation.GetResource();
	D3D12_RESOURCE_DESC const& SourceBufferDesc = pSourceResource->GetDesc();

	FD3D12Resource* pDestResource = DestBuffer->ResourceLocation.GetResource();
	D3D12_RESOURCE_DESC const& DestBufferDesc = pDestResource->GetDesc();

	checkf(pSourceResource != pDestResource, TEXT("CopyBufferRegion cannot be used on the same resource. This can happen when both the source and the dest are suballocated from the same resource."));

	check(DstOffset + NumBytes <= DestBufferDesc.Width);
	check(SrcOffset + NumBytes <= SourceBufferDesc.Width);

	numCopies++;

	FScopedResourceBarrier ScopeResourceBarrierSource(CommandListHandle, pSourceResource, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, FD3D12DynamicRHI::ETransitionMode::Validate);
	FScopedResourceBarrier ScopeResourceBarrierDest(CommandListHandle, pDestResource, D3D12_RESOURCE_STATE_COPY_DEST, 0, FD3D12DynamicRHI::ETransitionMode::Validate);
	CommandListHandle.FlushResourceBarriers();

	CommandListHandle->CopyBufferRegion(pDestResource->GetResource(), DestBuffer->ResourceLocation.GetOffsetFromBaseOfResource() + DstOffset, pSourceResource->GetResource(), SourceBuffer->ResourceLocation.GetOffsetFromBaseOfResource() + SrcOffset, NumBytes);
	CommandListHandle.UpdateResidency(pDestResource);
	CommandListHandle.UpdateResidency(pSourceResource);

	Device->RegisterGPUWork(1);
}

void FD3D12CommandContext::RHICopyBufferRegions(const TArrayView<const FCopyBufferRegionParams> Params)
{
	// Batched buffer copy finds unique source and destination buffer resources, performs transitions
	// to copy source / dest state, then performs copies and finally restores original state.

	using FLocalResourceArray = TArray<FD3D12Resource*, TInlineAllocator<16, TMemStackAllocator<>>>;
	FLocalResourceArray SrcBuffers;
	FLocalResourceArray DstBuffers;

	SrcBuffers.Reserve(Params.Num());
	DstBuffers.Reserve(Params.Num());

	// Transition buffers to copy states
	for (auto& Param : Params)
	{
		FD3D12Buffer* SourceBuffer = RetrieveObject<FD3D12Buffer>(Param.SourceBuffer);
		FD3D12Buffer* DestBuffer = RetrieveObject<FD3D12Buffer>(Param.DestBuffer);
		check(SourceBuffer);
		check(DestBuffer);

		FD3D12Device* Device = SourceBuffer->GetParentDevice();
		check(Device == DestBuffer->GetParentDevice());
		check(Device == GetParentDevice());

		FD3D12Resource* pSourceResource = SourceBuffer->ResourceLocation.GetResource();
		FD3D12Resource* pDestResource = DestBuffer->ResourceLocation.GetResource();

		checkf(pSourceResource != pDestResource, TEXT("CopyBufferRegion cannot be used on the same resource. This can happen when both the source and the dest are suballocated from the same resource."));

		SrcBuffers.Add(pSourceResource);
		DstBuffers.Add(pDestResource);
	}

	Algo::Sort(SrcBuffers);
	Algo::Sort(DstBuffers);

	enum class EBatchCopyState
	{
		CopySource,
		CopyDest,
		FinalizeSource,
		FinalizeDest,
	};

	auto TransitionResources = [](FD3D12CommandListHandle& CommandListHandle, FLocalResourceArray& SortedResources, EBatchCopyState State)
	{
		const uint32 Subresource = 0; // Buffers only have one subresource

		FD3D12Resource* PrevResource = nullptr;
		for (FD3D12Resource* Resource : SortedResources)
		{
			if (Resource == PrevResource) continue; // Skip duplicate resource barriers

			const bool bUseDefaultState = !Resource->RequiresResourceStateTracking();

			D3D12_RESOURCE_STATES DesiredState = D3D12_RESOURCE_STATE_CORRUPT;
			D3D12_RESOURCE_STATES CurrentState = D3D12_RESOURCE_STATE_CORRUPT;
			switch (State)
			{
			case EBatchCopyState::CopySource:
				DesiredState = D3D12_RESOURCE_STATE_COPY_SOURCE;
				CurrentState = bUseDefaultState ? Resource->GetDefaultResourceState() : CurrentState;
				break;
			case EBatchCopyState::CopyDest:
				DesiredState = D3D12_RESOURCE_STATE_COPY_DEST;
				CurrentState = bUseDefaultState ? Resource->GetDefaultResourceState() : CurrentState;
				break;
			case EBatchCopyState::FinalizeSource:
				CurrentState = D3D12_RESOURCE_STATE_COPY_SOURCE;
				DesiredState = bUseDefaultState ? Resource->GetDefaultResourceState() : D3D12_RESOURCE_STATE_GENERIC_READ;
				break;
			case EBatchCopyState::FinalizeDest:
				CurrentState = D3D12_RESOURCE_STATE_COPY_DEST;
				DesiredState = bUseDefaultState ? Resource->GetDefaultResourceState() : D3D12_RESOURCE_STATE_GENERIC_READ;
				break;
			default:
				checkf(false, TEXT("Unexpected batch copy state"));
				break;
			}

			if (bUseDefaultState)
			{
				check(CurrentState != D3D12_RESOURCE_STATE_CORRUPT);
				CommandListHandle.AddTransitionBarrier(Resource, CurrentState, DesiredState, Subresource);
			}
			else
			{
				FD3D12DynamicRHI::TransitionResource(CommandListHandle, Resource, D3D12_RESOURCE_STATE_TBD, DesiredState, Subresource, FD3D12DynamicRHI::ETransitionMode::Validate);
			}

			PrevResource = Resource;
		}
	};

	// Ensure that all previously pending barriers have been processed to avoid incorrect/conflicting transitions for non-tracked resources
	CommandListHandle.FlushResourceBarriers();

	TransitionResources(CommandListHandle, SrcBuffers, EBatchCopyState::CopySource);
	TransitionResources(CommandListHandle, DstBuffers, EBatchCopyState::CopyDest);

	// Issue all copy source/dest barriers before performing actual copies
	CommandListHandle.FlushResourceBarriers();

	for (auto& Param : Params)
	{
		FD3D12Buffer* SourceBuffer = RetrieveObject<FD3D12Buffer>(Param.SourceBuffer);
		FD3D12Buffer* DestBuffer = RetrieveObject<FD3D12Buffer>(Param.DestBuffer);
		uint64 SrcOffset = Param.SrcOffset;
		uint64 DstOffset = Param.DstOffset;
		uint64 NumBytes = Param.NumBytes;

		FD3D12Device* Device = SourceBuffer->GetParentDevice();
		check(Device == DestBuffer->GetParentDevice());

		FD3D12Resource* pSourceResource = SourceBuffer->ResourceLocation.GetResource();
		D3D12_RESOURCE_DESC const& SourceBufferDesc = pSourceResource->GetDesc();

		FD3D12Resource* pDestResource = DestBuffer->ResourceLocation.GetResource();
		D3D12_RESOURCE_DESC const& DestBufferDesc = pDestResource->GetDesc();

		check(DstOffset + NumBytes <= DestBufferDesc.Width);
		check(SrcOffset + NumBytes <= SourceBufferDesc.Width);

		numCopies++;

		CommandListHandle->CopyBufferRegion(pDestResource->GetResource(), DestBuffer->ResourceLocation.GetOffsetFromBaseOfResource() + DstOffset, pSourceResource->GetResource(), SourceBuffer->ResourceLocation.GetOffsetFromBaseOfResource() + SrcOffset, NumBytes);
		CommandListHandle.UpdateResidency(pDestResource);
		CommandListHandle.UpdateResidency(pSourceResource);

		Device->RegisterGPUWork(1);
	}

	// Transition buffers back to default readable state

	TransitionResources(CommandListHandle, SrcBuffers, EBatchCopyState::FinalizeSource);
	TransitionResources(CommandListHandle, DstBuffers, EBatchCopyState::FinalizeDest);
}
#endif // D3D12_RHI_RAYTRACING
