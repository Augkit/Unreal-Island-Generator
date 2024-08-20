#pragma once

#include "CoreMinimal.h"
#include "clipper.core.h"
#include "clipper.offset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Clipper2Helper.generated.h"

template <typename T>
using TClipperPath = Clipper2Lib::Path<T>;
template <typename T>
using TClipperPaths = Clipper2Lib::Paths<T>;

using FClipperPath64 = TClipperPath<int64_t>;
using FClipperPaths64 = TClipperPaths<int64_t>;

using FClipperPathD = TClipperPath<double>;
using FClipperPathsD = TClipperPaths<double>;

UENUM()
enum class EClipperJoinType : uint8
{
	Square = Clipper2Lib::JoinType::Square,
	Bevel = Clipper2Lib::JoinType::Bevel,
	Round = Clipper2Lib::JoinType::Round,
	Miter = Clipper2Lib::JoinType::Miter
};

UENUM()
enum class EClipperEndType : uint8
{
	Polygon = Clipper2Lib::EndType::Polygon,
	Joined = Clipper2Lib::EndType::Joined,
	Butt = Clipper2Lib::EndType::Butt,
	Square = Clipper2Lib::EndType::Square,
	Round = Clipper2Lib::EndType::Round
};

UCLASS()
class CLIPPER2_API UClipper2Helper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static void Offset(TArray<FVector2D>& OutResult, const TArray<FVector2D>& Points,
	                   const double Delta = 10., const double MiterLimit = 5.);

	static void Offset(FClipperPaths64& OutPaths, const FClipperPaths64& InPaths, double Delta = 10.,
	                   const EClipperJoinType Miter = EClipperJoinType::Miter, double MiterLimit = 5.,
	                   const EClipperEndType EndType = EClipperEndType::Polygon);

	static void Offset(FClipperPathsD& OutPaths, const FClipperPathsD& InPaths, double Delta = 10.,
	                   const EClipperJoinType Miter = EClipperJoinType::Miter, double MiterLimit = 5.,
	                   const EClipperEndType EndType = EClipperEndType::Polygon, int32 Precision = 2);

	static void Union(TArray<FVector2D>& OutResult, const TArray<FVector2D>& APoints, const TArray<FVector2D>& BPoints);

	static void Union(FClipperPaths64& OutPaths, const FClipperPaths64& InPaths);

	static void Union(FClipperPathsD& OutPaths, const FClipperPathsD& InPaths, int32 Precision = 2);

	static void Union(FClipperPathsD& OutPaths, const FClipperPathD& APath, const FClipperPathD& BPath,
	                  int32 Precision = 2);

	static void Union(FClipperPathsD& OutPaths, const FClipperPathsD& APaths, const FClipperPathsD& BPaths,
	                  int32 Precision = 2);

	static void InflatePaths(TArray<FVector2D>& OutResult, const TArray<FVector2D>& Points, double Delta,
	                         double MiterLimit = 5.);

	static void InflatePaths(FClipperPaths64& OutPaths, const FClipperPaths64& InPaths, double Delta = 10.,
	                         const EClipperJoinType Miter = EClipperJoinType::Miter, double MiterLimit = 5.,
	                         const EClipperEndType EndType = EClipperEndType::Polygon, double ArcTolerance = 0.);

	static void InflatePaths(FClipperPathsD& OutPaths, const FClipperPathsD& InPaths, double Delta = 10.,
	                         const EClipperJoinType Miter = EClipperJoinType::Miter, double MiterLimit = 5.,
	                         const EClipperEndType EndType = EClipperEndType::Polygon, double ArcTolerance = 0., 
	                         int32 Precision = 2);

	static void InflatePaths(FClipperPathsD& OutPaths, const FClipperPathD& InPath, double Delta = 10.,
	                         const EClipperJoinType Miter = EClipperJoinType::Miter, double MiterLimit = 2.,
	                         const EClipperEndType EndType = EClipperEndType::Polygon, double ArcTolerance = 0.,
	                         int32 Precision = 2);

	template <typename RealType>
	static void GetLongestPath(TArray<FVector2D>& OutResult, const TClipperPaths<RealType>& Paths)
	{
		int32 PathsSize = Paths.size();
		const Clipper2Lib::Path<RealType>* PathPtr = nullptr;
		OutResult.Empty(PathsSize);
		if (PathsSize <= 0)
		{
			return;
		}
		if (PathsSize == 1)
		{
			PathPtr = &Paths[0];
		}
		else
		{
			int32 MaxLengthPathSize = 0;
			for (int32 Index = 0; Index < PathsSize; ++Index)
			{
				if (Paths[Index].size() > MaxLengthPathSize)
				{
					MaxLengthPathSize = Paths[Index].size();
					PathPtr = &Paths[Index];
				}
			}
		}
		int32 PathSize = PathPtr->size();
		for (int32 Index = 0; Index < PathSize; ++Index)
		{
			OutResult.Add(FVector2D((*PathPtr)[Index].x, (*PathPtr)[Index].y));
		}
	}

	template <typename RealType, typename OutputType>
	static TClipperPaths<OutputType> ConvertPolygonsToPaths(
		const TArray<TArrayView<UE::Math::TVector2<RealType>>>& InPolygons,
		const UE::Math::TVector2<RealType>& InMin = {}, const RealType& InRange = {});

	template <typename RealType, typename OutputType>
	static TClipperPath<OutputType> ConvertPolygonToPath(const TArrayView<UE::Math::TVector2<RealType>>& InPolygon,
	                                                     const UE::Math::TVector2<RealType>& InMin = {},
	                                                     const RealType& InRange = {});

	template <typename RealType>
	FORCEINLINE static TClipperPath<RealType> MakePath(const TArray<FVector2D>& Points)
	{
		Clipper2Lib::Path<RealType> Result;
		for (const FVector2D& Point : Points)
		{
			Result.push_back(Clipper2Lib::Point<RealType>(Point.X, Point.Y));
		}
		return Result;
	}
};
