#pragma once

#include <limits>

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatform.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "PolyPartitionLib.h"
#include "PolyPartitionHelper.generated.h"

USTRUCT(BlueprintType)
struct FPolyTriangle2D
{
	GENERATED_BODY()
	FVector2D V0;
	int32 V0Index;
	FVector2D V1;
	int32 V1Index;
	FVector2D V2;
	int32 V2Index;

	FPolyTriangle2D() = default;

	FPolyTriangle2D(FVector2D InV0, int32 InV0Index, FVector2D InV1, int32 InV1Index, FVector2D InV2, int32 InV2Index
	) : V0(InV0), V0Index(InV0Index), V1(InV1), V1Index(InV1Index), V2(InV2), V2Index(InV2Index)
	{
	}
};

UCLASS()
class POLYPARTITION_API UPolyPartitionHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

protected:
	static TPPLPartition PolyPartition;

public:
	UFUNCTION(BlueprintCallable, Category = "Procedural Generation|PolyPartition")
	static void Triangulate(const TArray<FVector2D>& Points, const TArray<int32>& PointId,
	                        TArray<FPolyTriangle2D>& Triangles);

	UFUNCTION(BlueprintCallable, Category = "Procedural Generation|PolyPartition")
	static void TriangulateWithHole(
		const TArray<FVector2D>& Points, const TArray<int32>& PointId,
		const TArray<FVector2D>& HolePoints, const TArray<int32>& HolePointId,
		TArray<FPolyTriangle2D>& Triangles
	);

	static TPPLPoly MakePoly(const TArray<FVector2D>& Points, const TArray<int32>& PointId, bool bIsHole = false);
};
