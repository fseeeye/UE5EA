// Copyright Epic Games, Inc. All Rights Reserved.

#include <exception>
#include "OpenExrWrapper.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

PRAGMA_DEFAULT_VISIBILITY_START
THIRD_PARTY_INCLUDES_START
	#include "openexr/ImathBox.h"
	#include "openexr/ImfHeader.h"
	#include "openexr/ImfRgbaFile.h"
	#include "openexr/ImfCompressionAttribute.h"
	#include "openexr/ImfStandardAttributes.h"
THIRD_PARTY_INCLUDES_END
PRAGMA_DEFAULT_VISIBILITY_END

DECLARE_LOG_CATEGORY_EXTERN(LogOpenEXRWrapper, Log, All);
DEFINE_LOG_CATEGORY(LogOpenEXRWrapper);


/* FOpenExr
 *****************************************************************************/

void FOpenExr::SetGlobalThreadCount(uint16 ThreadCount)
{
	Imf::setGlobalThreadCount(ThreadCount);
}


/* FRgbaInputFile
 *****************************************************************************/

FRgbaInputFile::FRgbaInputFile(const FString& FilePath)
{
	try
	{
		InputFile = new Imf::RgbaInputFile(TCHAR_TO_ANSI(*FilePath));
	}
	catch (std::exception const& Exception)
	{
		UE_LOG(LogOpenEXRWrapper, Error, TEXT("Cannot load EXR file: %s"), StringCast<TCHAR>(Exception.what()).Get());
		InputFile = nullptr;
	}
}


FRgbaInputFile::FRgbaInputFile(const FString& FilePath, uint16 ThreadCount)
{
	try
	{
		InputFile = new Imf::RgbaInputFile(TCHAR_TO_ANSI(*FilePath), ThreadCount);
	}
	catch (std::exception const& Exception)
	{
		UE_LOG(LogOpenEXRWrapper, Error, TEXT("Cannot load EXR file: %s"), StringCast<TCHAR>(Exception.what()).Get());
		InputFile = nullptr;
	}
}


FRgbaInputFile::~FRgbaInputFile()
{
	delete (Imf::RgbaInputFile*)InputFile;
}


const TCHAR* FRgbaInputFile::GetCompressionName() const
{
	auto CompressionAttribute = ((Imf::RgbaInputFile*)InputFile)->header().findTypedAttribute<Imf::CompressionAttribute>("compression");

	if (CompressionAttribute == nullptr)
	{
		return TEXT("");
	}

	Imf::Compression Compression = CompressionAttribute->value();

	switch (Compression)
	{
	case Imf::NO_COMPRESSION:
		return TEXT("Uncompressed");

	case Imf::RLE_COMPRESSION:
		return TEXT("RLE");

	case Imf::ZIPS_COMPRESSION:
		return TEXT("ZIPS");

	case Imf::ZIP_COMPRESSION:
		return TEXT("ZIP");

	case Imf::PIZ_COMPRESSION:
		return TEXT("PIZ");

	case Imf::PXR24_COMPRESSION:
		return TEXT("PXR24");

	case Imf::B44_COMPRESSION:
		return TEXT("B44");

	case Imf::B44A_COMPRESSION:
		return TEXT("B44A");

	default:
		return TEXT("Unknown");
	}
}


FIntPoint FRgbaInputFile::GetDataWindow() const
{
	Imath::Box2i Win = ((Imf::RgbaInputFile*)InputFile)->dataWindow();

	return FIntPoint(
		Win.max.x - Win.min.x + 1,
		Win.max.y - Win.min.y + 1
	);
}


FFrameRate FRgbaInputFile::GetFrameRate(const FFrameRate& DefaultValue) const
{
	auto Attribute = ((Imf::RgbaInputFile*)InputFile)->header().findTypedAttribute<Imf::RationalAttribute>("framesPerSecond");

	if (Attribute == nullptr)
	{
		return DefaultValue;
	}

	const Imf::Rational& Value = Attribute->value();

	return FFrameRate(Value.n, Value.d);
}

int32 FRgbaInputFile::GetNumChannels() const
{
	Imf::RgbaChannels Channels = ((Imf::RgbaInputFile*)InputFile)->channels();
	int32 NumChannels = 3;
	switch (Channels)
	{
	case Imf::RgbaChannels::WRITE_RGB:
		NumChannels = 3;
		break;
	case Imf::RgbaChannels::WRITE_RGBA:
		NumChannels = 4;
		break;
	default:
		break;
	}
	return NumChannels;
}

int32 FRgbaInputFile::GetUncompressedSize() const
{
	const int32 NumChannels = ((Imf::RgbaInputFile*)InputFile)->channels();
	const int32 ChannelSize = sizeof(int16);
	const FIntPoint Window = GetDataWindow();

	return (Window.X * Window.Y * NumChannels * ChannelSize);
}


bool FRgbaInputFile::IsComplete() const
{
	return ((Imf::RgbaInputFile*)InputFile)->isComplete();
}


bool FRgbaInputFile::HasInputFile() const
{
	return InputFile != nullptr;
}


void FRgbaInputFile::ReadPixels(int32 StartY, int32 EndY)
{
	try
	{
		((Imf::RgbaInputFile*)InputFile)->readPixels(StartY, EndY);
	}
	catch (std::exception const& Exception)
	{
		UE_LOG(LogOpenEXRWrapper, Error, TEXT("Cannot read EXR file: %s (%s)"),
			ANSI_TO_TCHAR(((Imf::RgbaInputFile*)InputFile)->fileName()),
			StringCast<TCHAR>(Exception.what()).Get());
	}
}


void FRgbaInputFile::SetFrameBuffer(void* Buffer, const FIntPoint& BufferDim)
{
	Imath::Box2i Win = ((Imf::RgbaInputFile*)InputFile)->dataWindow();
	((Imf::RgbaInputFile*)InputFile)->setFrameBuffer((Imf::Rgba*)Buffer - Win.min.x - Win.min.y * BufferDim.X, 1, BufferDim.X);
}


IMPLEMENT_MODULE(FDefaultModuleImpl, OpenExrWrapper);
