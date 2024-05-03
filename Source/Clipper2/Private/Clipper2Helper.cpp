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
	Paths = SimplifyPaths(Paths, 2.5);
	Paths = Union(Paths, Clipper2Lib::FillRule::NonZero);
	Clipper2Lib::Path64 Path = Paths.back();

	OutResult.Empty(Path.size());
	for (const Clipper2Lib::Point64& Point : Path)
	{
		OutResult.Add(FVector2D(Point.x, Point.y));
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
