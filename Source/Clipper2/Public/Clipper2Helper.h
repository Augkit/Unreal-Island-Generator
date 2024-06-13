// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "clipper.core.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Clipper2Helper.generated.h"

/**
 * 
 */
UCLASS()
class CLIPPER2_API UClipper2Helper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static void Offset(TArray<FVector2D>& OutResult, const TArray<FVector2D>& Points, double Delta = 10,
	                   double MiterLimit = 5);

	static void Union(TArray<FVector2D>& OutResult, const TArray<FVector2D>& APoints, const TArray<FVector2D>& BPoints);

	static void GetLongestPath(TArray<FVector2D>& OutResult, const Clipper2Lib::Paths64& Paths);
	
	template <typename RealType, typename OutputType>
	static Clipper2Lib::Paths<OutputType> ConvertPolygonsToPaths(
		const TArray<TArrayView<UE::Math::TVector2<RealType>>>& InPolygons,
		const UE::Math::TVector2<RealType>& InMin = {}, const RealType& InRange = {});

	template <typename RealType, typename OutputType>
	static Clipper2Lib::Path<OutputType> ConvertPolygonToPath(const TArrayView<UE::Math::TVector2<RealType>>& InPolygon,
	                                                          const UE::Math::TVector2<RealType>& InMin = {},
	                                                          const RealType& InRange = {});

	template <typename OutputType>
	static Clipper2Lib::Path<OutputType> MakePath(const TArray<FVector2D>& Points);
};
