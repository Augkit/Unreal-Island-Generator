// Fill out your copyright notice in the Description page of Project Settings.


#include "Clipper2Helper.h"

#include "clipper.core.h"
#include "clipper.h"

void UClipper2Helper::Offset(TArray<FVector2D>& OutResult, const TArray<FVector2D>& Points, double Delta,
                             double MiterLimit)
{
	Clipper2Lib::Path64 Subject = MakePath<int64_t>(Points);
	Clipper2Lib::ClipperOffset ClipperOffset;
	ClipperOffset.AddPath(Subject, Clipper2Lib::JoinType::Miter, Clipper2Lib::EndType::Polygon);
	ClipperOffset.MiterLimit(MiterLimit);
	Clipper2Lib::Paths64 Paths;
	ClipperOffset.Execute(Delta, Paths);
	// Paths = SimplifyPaths(Paths, 2.5);
	// Paths = Clipper2Lib::Union(Paths, Clipper2Lib::FillRule::NonZero);
	GetLongestPath(OutResult, Paths);
}

void UClipper2Helper::Union(TArray<FVector2D>& OutResult, const TArray<FVector2D>& APoints,
	const TArray<FVector2D>& BPoints)
{
	Clipper2Lib::Path64 ASubject = MakePath<int64_t>(APoints);
	Clipper2Lib::Path64 BSubject = MakePath<int64_t>(BPoints);
	Clipper2Lib::Paths64 Paths;
	Paths.push_back(ASubject);
	Paths.push_back(BSubject);
	Paths = Clipper2Lib::Union(Paths, Clipper2Lib::FillRule::NonZero);
	GetLongestPath(OutResult, Paths);
}

void UClipper2Helper::GetLongestPath(TArray<FVector2D>& OutResult, const Clipper2Lib::Paths64& Paths)
{
	int32 PathsSize = Paths.size();
	const Clipper2Lib::Path64* PathPtr = nullptr;
	OutResult.Empty(PathsSize);
	if (PathsSize < 0)
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
	for(int32 Index = 0; Index < PathSize; ++ Index)
	{
		OutResult.Add(FVector2D((*PathPtr)[Index].x, (*PathPtr)[Index].y));
	}
}

template <typename RealType, typename OutputType>
Clipper2Lib::Paths<OutputType> UClipper2Helper::ConvertPolygonsToPaths(
	const TArray<TArrayView<UE::Math::TVector2<RealType>>>& InPolygons,
	const UE::Math::TVector2<RealType>& InMin,
	const RealType& InRange)
{
	Clipper2Lib::Paths<OutputType> Paths;
	Paths.reserve(InPolygons.Num());
	for (const TArrayView<UE::Math::TVector2<RealType>>& Polygon : InPolygons)
	{
		Paths.push_back(ConvertPolygonToPath<RealType, OutputType>(Polygon, InMin, InRange));
	}
	return Paths;
}

template <typename RealType, typename OutputType>
Clipper2Lib::Path<OutputType> UClipper2Helper::ConvertPolygonToPath(
	const TArrayView<UE::Math::TVector2<RealType>>& InPolygon,
	const UE::Math::TVector2<RealType>& InMin,
	const RealType& InRange)
{
	Clipper2Lib::Path<OutputType> Path;
	Path.reserve(InPolygon.Num());
	for (int32 Idx = 0; Idx < InPolygon.Num(); ++Idx)
	{
		// Integral output normalized the input floating point values
		if constexpr (TIsIntegral<OutputType>::Value)
		{
			Path.emplace_back(PackVector<RealType, OutputType>(InPolygon[Idx], InMin, InRange));
		}
		else if constexpr (TIsFloatingPoint<OutputType>::Value)
		{
			Path.emplace_back(InPolygon[Idx].X, InPolygon[Idx].Y);
		}
	}
	return Path;
}

template <typename OutputType>
Clipper2Lib::Path<OutputType> UClipper2Helper::MakePath(const TArray<FVector2D>& Points)
{
	Clipper2Lib::Path<OutputType> Result;
	for (const FVector2D& Point : Points)
	{
		Result.push_back(Clipper2Lib::Point<OutputType>(Point.X, Point.Y));
	}
	return Result;
}
