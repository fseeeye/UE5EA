// Copyright Epic Games, Inc. All Rights Reserved.
#include "CADData.h"

#include "CADOptions.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

uint32 MeshArchiveMagic = 345612;

namespace CADLibrary
{
uint32 BuildColorId(uint32 ColorId, uint8 Alpha)
{
	if (Alpha == 0)
	{
		Alpha = 1;
	}
	return ColorId | Alpha << 24;
}

void GetCTColorIdAlpha(ColorId ColorId, uint32& CTColorId, uint8& Alpha)
{
	CTColorId = ColorId & 0x00ffffff;
	Alpha = (uint8)((ColorId & 0xff000000) >> 24);
}

int32 BuildColorName(const FColor& Color)
{
	FString Name = FString::Printf(TEXT("%02x%02x%02x%02x"), Color.R, Color.G, Color.B, Color.A);
	return FGenericPlatformMath::Abs((int32)GetTypeHash(Name));
}

int32 BuildMaterialName(const FCADMaterial& Material)
{
	FString Name;
	if (!Material.MaterialName.IsEmpty())
	{
		Name += Material.MaterialName;  // we add material name because it could be used by the end user so two material with same parameters but different name are different.
	}
	Name += FString::Printf(TEXT("%02x%02x%02x "), Material.Diffuse.R, Material.Diffuse.G, Material.Diffuse.B);
	Name += FString::Printf(TEXT("%02x%02x%02x "), Material.Ambient.R, Material.Ambient.G, Material.Ambient.B);
	Name += FString::Printf(TEXT("%02x%02x%02x "), Material.Specular.R, Material.Specular.G, Material.Specular.B);
	Name += FString::Printf(TEXT("%02x%02x%02x"), (int)(Material.Shininess * 255.0), (int)(Material.Transparency * 255.0), (int)(Material.Reflexion * 255.0));

	if (!Material.TextureName.IsEmpty())
	{
		Name += Material.TextureName;
	}
	return FMath::Abs((int32)GetTypeHash(Name));
}

FArchive& operator<<(FArchive& Ar, FCADMaterial& Material)
{
	Ar << Material.MaterialName;
	Ar << Material.Diffuse;
	Ar << Material.Ambient;
	Ar << Material.Specular;
	Ar << Material.Shininess;
	Ar << Material.Transparency;
	Ar << Material.Reflexion;
	Ar << Material.TextureName;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FFileDescription& File)
{
	Ar << File.Path;
	Ar << File.Name;
	Ar << File.Extension;
	Ar << File.Configuration;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FTessellationData& TessellationData)
{
	Ar << TessellationData.PositionArray;

	Ar << TessellationData.PositionIndices;
	Ar << TessellationData.VertexIndices;

	Ar << TessellationData.NormalArray;
	Ar << TessellationData.TexCoordArray;

	Ar << TessellationData.ColorName;
	Ar << TessellationData.MaterialName;

	Ar << TessellationData.PatchId;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FBodyMesh& BodyMesh)
{
	Ar << BodyMesh.VertexArray;
	Ar << BodyMesh.Faces;
	Ar << BodyMesh.BBox;

	Ar << BodyMesh.TriangleCount;
	Ar << BodyMesh.BodyID;
	Ar << BodyMesh.MeshActorName;

	Ar << BodyMesh.MaterialSet;
	Ar << BodyMesh.ColorSet;

	return Ar;
}

void SerializeBodyMeshSet(const TCHAR* Filename, TArray<FBodyMesh>& InBodySet)
{
	TUniquePtr<FArchive> Archive(IFileManager::Get().CreateFileWriter(Filename));

	*Archive << MeshArchiveMagic;

	*Archive << InBodySet;

	Archive->Close();
}

void DeserializeBodyMeshFile(const TCHAR* Filename, TArray<FBodyMesh>& OutBodySet)
{
	TUniquePtr<FArchive> Archive(IFileManager::Get().CreateFileReader(Filename));

	uint32 MagicNumber = 0;
	*Archive << MagicNumber;
	if (MagicNumber == MeshArchiveMagic)
	{
		*Archive << OutBodySet;
	}
	Archive->Close();
}

// Duplicated with FDatasmithUtils::GetCleanFilenameAndExtension, to delete as soon as possible
void GetCleanFilenameAndExtension(const FString& InFilePath, FString& OutFilename, FString& OutExtension)
{
	if (InFilePath.IsEmpty())
	{
		OutFilename.Empty();
		OutExtension.Empty();
		return;
	}

	FString BaseFile = FPaths::GetCleanFilename(InFilePath);
	BaseFile.Split(TEXT("."), &OutFilename, &OutExtension, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

	if (!OutExtension.IsEmpty() && FCString::IsNumeric(*OutExtension))
	{
		BaseFile = OutFilename;
		BaseFile.Split(TEXT("."), &OutFilename, &OutExtension, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		OutExtension = OutExtension + TEXT(".*");
		return;
	}

	if (OutExtension.IsEmpty())
	{
		OutFilename = BaseFile;
	}
}

}
