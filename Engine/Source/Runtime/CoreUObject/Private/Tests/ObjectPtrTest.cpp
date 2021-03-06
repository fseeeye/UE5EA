// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/ObjectPtr.h"

#include "ObjectRefTrackingTestBase.h"

#include "Concepts/EqualityComparable.h"
#include "Serialization/ArchiveCountMem.h"
#include "Templates/Models.h"
#include "UObject/Interface.h"
#include "UObject/SoftObjectPath.h"

#include <type_traits>

namespace ObjectPtrTest
{
using FMutableObjectPtr = TObjectPtr<UObject>;
using FMutableInterfacePtr = TObjectPtr<UInterface>;
using FMutablePackagePtr = TObjectPtr<UPackage>;
using FConstObjectPtr = TObjectPtr<const UObject>;
using FConstInterfacePtr = TObjectPtr<const UInterface>;
using FConstPackagePtr = TObjectPtr<const UPackage>;

class UForwardDeclaredObjDerived;
class FForwardDeclaredNotObjDerived;

static_assert(sizeof(FObjectPtr) == sizeof(FObjectHandle), "FObjectPtr type must always compile to something equivalent to an FObjectHandle size.");
static_assert(sizeof(FObjectPtr) == sizeof(void*), "FObjectPtr type must always compile to something equivalent to a pointer size.");
static_assert(sizeof(TObjectPtr<UObject>) == sizeof(void*), "TObjectPtr<UObject> type must always compile to something equivalent to a pointer size.");

// Ensure that a TObjectPtr is trivially copyable, (copy/move) constructible, (copy/move) assignable, and destructible
static_assert(std::is_trivially_copyable<FMutableObjectPtr>::value, "TObjectPtr must be trivially copyable");
static_assert(std::is_trivially_copy_constructible<FMutableObjectPtr>::value, "TObjectPtr must be trivially copy constructible");
static_assert(std::is_trivially_move_constructible<FMutableObjectPtr>::value, "TObjectPtr must be trivially move constructible");
static_assert(std::is_trivially_copy_assignable<FMutableObjectPtr>::value, "TObjectPtr must be trivially copy assignable");
static_assert(std::is_trivially_move_assignable<FMutableObjectPtr>::value, "TObjectPtr must be trivially move assignable");
static_assert(std::is_trivially_destructible<FMutableObjectPtr>::value, "TObjectPtr must be trivially destructible");
static_assert(std::is_trivially_default_constructible<FMutableObjectPtr>::value, "TObjectPtr must be trivially default constructible");

// Ensure that raw pointers can be used to construct wrapped object pointers and that const-ness isn't stripped when constructing or converting with raw pointers
static_assert(std::is_constructible<FMutableObjectPtr, UObject*>::value, "TObjectPtr<UObject> must be constructible from a raw UObject*");
static_assert(!std::is_constructible<FMutableObjectPtr, const UObject*>::value, "TObjectPtr<UObject> must not be constructible from a const raw UObject*");
static_assert(std::is_convertible<FMutableObjectPtr, UObject*>::value, "TObjectPtr<UObject> must be convertible to a raw UObject*");
static_assert(std::is_convertible<FMutableObjectPtr, const UObject*>::value, "TObjectPtr<UObject> must be convertible to a const raw UObject*");

static_assert(std::is_constructible<FConstObjectPtr, UObject*>::value, "TObjectPtr<const UObject> must be constructible from a raw UObject*");
static_assert(std::is_constructible<FConstObjectPtr, const UObject*>::value, "TObjectPtr<const UObject> must be constructible from a const raw UObject*");
static_assert(!std::is_convertible<FConstObjectPtr, UObject*>::value, "TObjectPtr<const UObject> must not be convertible to a raw UObject*");
static_assert(std::is_convertible<FConstObjectPtr, const UObject*>::value, "TObjectPtr<const UObject> must be convertible to a const raw UObject*");

// Ensure that a TObjectPtr<const UObject> is constructible and assignable from a TObjectPtr<UObject> but not vice versa
static_assert(std::is_constructible<FConstObjectPtr, const FMutableObjectPtr&>::value, "Missing constructor (TObjectPtr<const UObject> from TObjectPtr<UObject>)");
static_assert(!std::is_constructible<FMutableObjectPtr, const FConstObjectPtr&>::value, "Invalid constructor (TObjectPtr<UObject> from TObjectPtr<const UObject>)");
static_assert(std::is_assignable<FConstObjectPtr, const FMutableObjectPtr&>::value, "Missing assignment (TObjectPtr<const UObject> from TObjectPtr<UObject>)");
static_assert(!std::is_assignable<FMutableObjectPtr, const FConstObjectPtr&>::value, "Invalid assignment (TObjectPtr<UObject> from TObjectPtr<const UObject>)");

static_assert(std::is_constructible<FConstObjectPtr, const FConstObjectPtr&>::value, "Missing constructor (TObjectPtr<const UObject> from TObjectPtr<const UObject>)");
static_assert(std::is_assignable<FConstObjectPtr, const FConstObjectPtr&>::value, "Missing assignment (TObjectPtr<const UObject> from TObjectPtr<const UObject>)");

// Ensure that a TObjectPtr<UObject> is constructible and assignable from a TObjectPtr<UInterface> but not vice versa
static_assert(std::is_constructible<FMutableObjectPtr, const FMutableInterfacePtr&>::value, "Missing constructor (TObjectPtr<UObject> from TObjectPtr<UInterface>)");
static_assert(!std::is_constructible<FMutableInterfacePtr, const FMutableObjectPtr&>::value, "Invalid constructor (TObjectPtr<UInterface> from TObjectPtr<UObject>)");
static_assert(std::is_constructible<FConstObjectPtr, const FConstInterfacePtr&>::value, "Missing constructor (TObjectPtr<const UObject> from TObjectPtr<const UInterface>)");
static_assert(std::is_constructible<FConstObjectPtr, const FMutableInterfacePtr&>::value, "Missing constructor (TObjectPtr<const UObject> from TObjectPtr<UInterface>)");
static_assert(!std::is_constructible<FConstInterfacePtr, const FConstObjectPtr&>::value, "Invalid constructor (TObjectPtr<const UInterface> from TObjectPtr<const UObject>)");
static_assert(!std::is_constructible<FConstInterfacePtr, const FMutableObjectPtr&>::value, "Invalid constructor (TObjectPtr<const UInterface> from TObjectPtr<UObject>)");

static_assert(std::is_assignable<FMutableObjectPtr, const FMutableInterfacePtr&>::value, "Missing assignment (TObjectPtr<UObject> from TObjectPtr<UInterface>)");
static_assert(std::is_assignable<FConstObjectPtr, const FMutableInterfacePtr&>::value, "Missing assignment (TObjectPtr<const UObject> from TObjectPtr<UInterface>)");
static_assert(std::is_assignable<FConstObjectPtr, const FConstInterfacePtr&>::value, "Missing assignment (TObjectPtr<const UObject> from TObjectPtr<const UInterface>)");
static_assert(!std::is_assignable<FMutableInterfacePtr, const FMutableObjectPtr&>::value, "Invalid assignment (TObjectPtr<UInterface> from TObjectPtr<UObject>)");
static_assert(!std::is_assignable<FConstInterfacePtr, const FMutableObjectPtr&>::value, "Invalid assignment (TObjectPtr<const UInterface> from TObjectPtr<UObject>)");
static_assert(!std::is_assignable<FConstInterfacePtr, const FConstObjectPtr&>::value, "Invalid assignment (TObjectPtr<const UInterface> from TObjectPtr<const UObject>)");

// Ensure that TObjectPtr<[const] UObject> is comparable with another TObjectPtr<[const] UObject> regardless of constness
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, FConstObjectPtr>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and TObjectPtr<const UObject>");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, FConstObjectPtr>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and TObjectPtr<const UObject>");

// Ensure that TObjectPtr<[const] UObject> is comparable with another TObjectPtr<[const] UInterface> regardless of constness
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, FConstInterfacePtr>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and TObjectPtr<const UInterface>");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, FConstInterfacePtr>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and TObjectPtr<const UInterface>");
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, FMutableInterfacePtr>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and TObjectPtr<UInterface>");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, FMutableInterfacePtr>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and TObjectPtr<UInterface>");

// Ensure that TObjectPtr<[const] UPackage> is not comparable with a TObjectPtr<[const] UInterface> regardless of constness
// TODO: This only ensures that at least one of the A==B,B==A,A!=B,B!=A operations fail, not that they all fail.
#if !(PLATFORM_MICROSOFT) || !defined(_MSC_EXTENSIONS) // MSVC static analyzer is run in non-conformance mode, and that causes these checks to fail.
static_assert(!TModels<CEqualityComparableWith, FConstPackagePtr, FConstInterfacePtr>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<const UPackage> and TObjectPtr<const UInterface>");
static_assert(!TModels<CEqualityComparableWith, FMutablePackagePtr, FConstInterfacePtr>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<UPackage> and TObjectPtr<const UInterface>");
static_assert(!TModels<CEqualityComparableWith, FConstPackagePtr, FMutableInterfacePtr>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<const UPackage> and TObjectPtr<UInterface>");
static_assert(!TModels<CEqualityComparableWith, FMutablePackagePtr, FMutableInterfacePtr>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<UPackage> and TObjectPtr<UInterface>");
#endif // #if !(PLATFORM_MICROSOFT) || !defined(_MSC_EXTENSIONS)

// Ensure that TObjectPtr<[const] UObject> is comparable with a raw pointer of the same referenced type regardless of constness
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, const UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and const UObject*");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, const UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and const UObject*");
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and UObject*");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and UObject*");

// Ensure that TObjectPtr<[const] UObject> is comparable with a UInterface raw pointer regardless of constness
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, const UInterface*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and const UInterface*");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, const UInterface*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and const UInterface*");
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, UInterface*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and UInterface*");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, UInterface*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and UInterface*");

// Ensure that TObjectPtr<[const] UInterface> is comparable with a UObject raw pointer regardless of constness
static_assert(TModels<CEqualityComparableWith, FConstInterfacePtr, const UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UInterface> and const UObject*");
static_assert(TModels<CEqualityComparableWith, FMutableInterfacePtr, const UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UInterface> and const UObject*");
static_assert(TModels<CEqualityComparableWith, FConstInterfacePtr, UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UInterface> and UObject*");
static_assert(TModels<CEqualityComparableWith, FMutableInterfacePtr, UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UInterface> and UObject*");

// Ensure that TObjectPtr<[const] UInterface> is not comparable with a UPackage raw pointer regardless of constness
// TODO: This only ensures that at least one of the A==B,B==A,A!=B,B!=A operations fail, not that they all fail.
static_assert(!TModels<CEqualityComparableWith, FConstInterfacePtr, const UPackage*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<const UInterface> and const UPackage*");
static_assert(!TModels<CEqualityComparableWith, FMutableInterfacePtr, const UPackage*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<UInterface> and const UPackage*");
static_assert(!TModels<CEqualityComparableWith, FConstInterfacePtr, UPackage*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<const UInterface> and UPackage*");
static_assert(!TModels<CEqualityComparableWith, FMutableInterfacePtr, UPackage*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<UInterface> and UPackage*");

// Ensure that TObjectPtr<[const] UInterface> is not comparable with a char raw pointer regardless of constness
// TODO: This only ensures that at least one of the A==B,B==A,A!=B,B!=A operations fail, not that they all fail.
static_assert(!TModels<CEqualityComparableWith, FConstObjectPtr, const char*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and const UObject*");
static_assert(!TModels<CEqualityComparableWith, FMutableObjectPtr, const char*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and const UObject*");
static_assert(!TModels<CEqualityComparableWith, FConstObjectPtr, char*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and UObject*");
static_assert(!TModels<CEqualityComparableWith, FMutableObjectPtr, char*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and UObject*");

// Ensure that TObjectPtr<[const] UObject> is comparable with nullptr regardless of constness
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, TYPE_OF_NULLPTR>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and nullptr");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, TYPE_OF_NULLPTR>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and nullptr");

#if !UE_OBJECT_PTR_NONCONFORMANCE_SUPPORT // Specialized NULL support causes these checks to fail.
static_assert(!TModels<CEqualityComparableWith, FConstObjectPtr, long>::Value, "Should not be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and long");
static_assert(!TModels<CEqualityComparableWith, FMutableObjectPtr, long>::Value, "Should not be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and long");
#endif // #if !UE_OBJECT_PTR_NONCONFORMANCE_SUPPORT

// Ensure that the use of incomplete types doesn't provide a means to bypass type safety on TObjectPtr
// NOTE: This is disabled because we're permitting this operation with a deprecation warning.
//static_assert(!std::is_assignable<TObjectPtr<UForwardDeclaredObjDerived>, UForwardDeclaredObjDerived*>::value, "Should not be able to assign raw pointer of incomplete type that descends from UObject to exactly this type of TObjectPtr");
//static_assert(!std::is_assignable<TObjectPtr<FForwardDeclaredNotObjDerived>, FForwardDeclaredNotObjDerived*>::value, "Should not be able to assign raw pointer of incomplete type that does not descend from UObject to exactly this type of TObjectPtr");

#if WITH_DEV_AUTOMATION_TESTS

class FObjectPtrTestBase : public FObjectRefTrackingTestBase
{
public:
	FObjectPtrTestBase(const FString& InName, const bool bInComplexTask)
	: FObjectRefTrackingTestBase(InName, bInComplexTask)
	{
	}

protected:
};

#define TEST_NAME_ROOT TEXT("System.CoreUObject.ObjectPtr")
constexpr const uint32 ObjectPtrTestFlags = EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter;

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FObjectPtrTestNullBehavior, FObjectPtrTestBase, TEST_NAME_ROOT TEXT(".NullBehavior"), ObjectPtrTestFlags)
bool FObjectPtrTestNullBehavior::RunTest(const FString& Parameters)
{
	TObjectPtr<UObject> NullObjectPtr(nullptr);
	TestTrue(TEXT("Nullptr should equal a null object pointer"), nullptr == NullObjectPtr);
	TestTrue(TEXT("A null object pointer should equal nullptr"), NullObjectPtr == nullptr);
	TestFalse(TEXT("A null object pointer should evaluate to false"), !!NullObjectPtr);
	TestTrue(TEXT("Negation of a null object pointer should evaluate to true"), !NullObjectPtr);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FObjectPtrTestDefaultSerialize, FObjectPtrTestBase, TEST_NAME_ROOT TEXT(".DefaultSerialize"), ObjectPtrTestFlags)
bool FObjectPtrTestDefaultSerialize::RunTest(const FString& Parameters)
{
	FSnapshotObjectRefMetrics ObjectRefMetrics(*this);
	FObjectPtr DefaultTexturePtr(FObjectRef {FName("/Engine/EngineResources/DefaultTexture"), NAME_None, NAME_None, FObjectPathId("DefaultTexture")});

	ObjectRefMetrics.TestNumResolves(TEXT("Unexpected resolve count after initializing an FObjectPtr"), UE_WITH_OBJECT_HANDLE_LATE_RESOLVE ? 0 : 1);
	ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after initializing an FObjectPtr"), 0);
	ObjectRefMetrics.TestNumReads(TEXT("NumReads should not change when initializing an FObjectPtr"), 0);

	FArchiveCountMem Writer(nullptr);
	Writer << DefaultTexturePtr;

	ObjectRefMetrics.TestNumResolves(TEXT("Serializing an FObjectPtr should force it to resolve"), 1);
	ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after serializing an FObjectPtr"), 0);
	ObjectRefMetrics.TestNumReads(TEXT("NumReads should increase after serializing an FObjectPtr"), 1);

	Writer << DefaultTexturePtr;

	ObjectRefMetrics.TestNumResolves(TEXT("Serializing an FObjectPtr twice should only require it to resolve once"), 1);
	ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after serializing an FObjectPtr"), 0);
	ObjectRefMetrics.TestNumReads(TEXT("NumReads should increase after serializing an FObjectPtr"), 2);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FObjectPtrTestSoftObjectPath, FObjectPtrTestBase, TEST_NAME_ROOT TEXT(".SoftObjectPath"), ObjectPtrTestFlags)
bool FObjectPtrTestSoftObjectPath::RunTest(const FString& Parameters)
{
	FSnapshotObjectRefMetrics ObjectRefMetrics(*this);
	FObjectPtr DefaultTexturePtr(FObjectRef {FName("/Engine/EngineResources/DefaultTexture"), NAME_None, NAME_None, FObjectPathId("DefaultTexture")});

	ObjectRefMetrics.TestNumResolves(TEXT("Unexpected resolve count after initializing an FObjectPtr"), UE_WITH_OBJECT_HANDLE_LATE_RESOLVE ? 0 : 1);
	ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after initializing an FObjectPtr"), 0);
	ObjectRefMetrics.TestNumReads(TEXT("NumReads should not change when initializing an FObjectPtr"), 0);

	FSoftObjectPath DefaultTexturePath(DefaultTexturePtr);

	ObjectRefMetrics.TestNumResolves(TEXT("Unexpected resolve count after initializing an FSoftObjectPath from an FObjectPtr"), UE_WITH_OBJECT_HANDLE_LATE_RESOLVE ? 0 : 1);

	TestEqual(TEXT("Soft object path constructed from an FObjectPtr does not have the expected path value"), *DefaultTexturePath.ToString(), TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FObjectPtrTestForwardDeclared, FObjectPtrTestBase, TEST_NAME_ROOT TEXT(".ForwardDeclared"), ObjectPtrTestFlags)
bool FObjectPtrTestForwardDeclared::RunTest(const FString& Parameters)
{
	UForwardDeclaredObjDerived* PtrFwd = nullptr;
	TObjectPtr<UForwardDeclaredObjDerived> ObjPtrFwd(MakeObjectPtrUnsafe<UForwardDeclaredObjDerived>(reinterpret_cast<UObject*>(PtrFwd)));
	TestTrue(TEXT("Null forward declared pointer used to construct a TObjectPtr should result in a null TObjectPtr"), ObjPtrFwd.IsNull());
	return true;
}

// @TODO: OBJPTR: We should have a test that ensures that we can (de)serialize an FObjectPtr to FLinkerSave/FLinkerLoad and that upon load the object
//			pointer is not resolved if we are in a configuration that supports lazy load.  This is proving difficult due to the restrictions around how
//			FLinkerSave/FLinkerLoad is used.
// IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FObjectPtrTestLinkerSerializeBehavior, FObjectPtrTestBase, TEST_NAME_ROOT TEXT(".LinkerSerialize"), ObjectPtrTestFlags)
// bool FObjectPtrTestLinkerSerializeBehavior::RunTest(const FString& Parameters)
// {
// 	FSnapshotObjectRefMetrics ObjectRefMetrics(*this);
// 	FObjectPtr DefaultTexturePtr(FObjectRef {FName("/Engine/EngineResources/DefaultTexture"), NAME_None, NAME_None, FObjectPathId("DefaultTexture")});

// 	TUniquePtr<FLinkerSave> Linker = TUniquePtr<FLinkerSave>(new FLinkerSave(nullptr /*InOuter*/, false /*bForceByteSwapping*/, true /*bSaveUnversioned*/));
// 	return true;
// }

#undef TEST_NAME_ROOT

#endif // WITH_DEV_AUTOMATION_TESTS

class UForwardDeclaredObjDerived: public UObject {};
class FForwardDeclaredNotObjDerived {};

}
