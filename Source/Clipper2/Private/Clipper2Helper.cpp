#include "Clipper2Helper.h"

#include "clipper.core.h"
#include "clipper.h"

void UClipper2Helper::Offset(TArray<FVector2D>& OutResult, const TArray<FVector2D>& Points, double Delta,
                             double MiterLimit)
{
	FClipperPaths64 InPaths;
	InPaths.push_back(MakePath<int64_t>(Points));
	FClipperPaths64 OutPaths;
	Offset(OutPaths, InPaths, Delta, EClipperJoinType::Miter, MiterLimit);
	GetLongestPath(OutResult, OutPaths);
}

void UClipper2Helper::Offset(FClipperPaths64& OutPaths, const FClipperPaths64& InPaths, double Delta,
                             const EClipperJoinType JoinType, double MiterLimit, const EClipperEndType EndType)
{
	Clipper2Lib::ClipperOffset ClipperOffset;
	ClipperOffset.AddPaths(InPaths, static_cast<Clipper2Lib::JoinType>(JoinType),
	                       static_cast<Clipper2Lib::EndType>(EndType));
	ClipperOffset.MiterLimit(MiterLimit);
	ClipperOffset.Execute(Delta, OutPaths);
}

void UClipper2Helper::Offset(FClipperPathsD& OutPaths, const FClipperPathsD& InPaths, double Delta,
                             const EClipperJoinType Miter, double MiterLimit, const EClipperEndType EndType,
                             int32 Precision)
{
	double Scale = FMath::Pow(static_cast<double>(10), Precision);
	FClipperPaths64 Paths64;
	Paths64.resize(InPaths.size());
	for (int32 PathsIndex = 0; PathsIndex < InPaths.size(); ++PathsIndex)
	{
		const FClipperPathD& PathD = InPaths[PathsIndex];
		FClipperPath64& Path64 = Paths64[PathsIndex];
		Path64.resize(PathD.size());
		for (int32 PathIndex = 0; PathIndex < PathD.size(); ++PathIndex)
		{
			Path64[PathIndex].x = PathD[PathIndex].x * Scale;
			Path64[PathIndex].y = PathD[PathIndex].y * Scale;
		}
	}
	FClipperPaths64 OutPaths64;
	Offset(OutPaths64, Paths64, Delta, Miter, MiterLimit, EndType);
	OutPaths.resize(OutPaths64.size());
	for (int32 OutPathsIndex = 0; OutPathsIndex < OutPaths64.size(); ++OutPathsIndex)
	{
		const FClipperPath64& Path64 = OutPaths64[OutPathsIndex];
		FClipperPathD& PathD = OutPaths[OutPathsIndex];
		PathD.resize(Path64.size());
		for (int32 PathIndex = 0; PathIndex < Path64.size(); ++PathIndex)
		{
			PathD[PathIndex].x = Path64[PathIndex].x / Scale;
			PathD[PathIndex].y = Path64[PathIndex].y / Scale;
		}
	}
}

void UClipper2Helper::Union(TArray<FVector2D>& OutResult, const TArray<FVector2D>& APoints,
                            const TArray<FVector2D>& BPoints)
{
	FClipperPathsD OutPaths;
	Union(OutPaths, MakePath<double>(APoints), MakePath<double>(BPoints));
	GetLongestPath(OutResult, OutPaths);
}

void UClipper2Helper::Union(FClipperPaths64& OutPaths, const FClipperPaths64& InPaths)
{
	OutPaths = Clipper2Lib::Union(InPaths, Clipper2Lib::FillRule::NonZero);
}

void UClipper2Helper::Union(FClipperPathsD& OutPaths, const FClipperPathsD& InPaths, int32 Precision)
{
	OutPaths = Clipper2Lib::Union(InPaths, Clipper2Lib::FillRule::Positive, Precision);
}

void UClipper2Helper::Union(FClipperPathsD& OutPaths, const FClipperPathD& APath, const FClipperPathD& BPath,
                            int32 Precision)
{
	FClipperPathsD Paths;
	Paths.push_back(APath);
	Paths.push_back(BPath);
	Union(OutPaths, Paths, Precision);
}

void UClipper2Helper::Union(FClipperPathsD& OutPaths, const FClipperPathsD& APaths, const FClipperPathsD& BPaths,
                            int32 Precision)
{
	FClipperPathsD Paths;
	Paths.insert(Paths.end(), APaths.begin(), APaths.end());
	Paths.insert(Paths.end(), BPaths.begin(), BPaths.end());
	Union(OutPaths, Paths, Precision);
}

void UClipper2Helper::InflatePaths(TArray<FVector2D>& OutResult, const TArray<FVector2D>& Points, double Delta,
                                   double MiterLimit)
{
	const Clipper2Lib::Path<double> Path = MakePath<double>(Points);
	Clipper2Lib::PathsD Paths;
	Paths.push_back(Path);
	GetLongestPath(
		OutResult,
		Clipper2Lib::InflatePaths(Paths, Delta, Clipper2Lib::JoinType::Square, Clipper2Lib::EndType::Butt, MiterLimit));
}

void UClipper2Helper::InflatePaths(FClipperPaths64& OutPaths, const FClipperPaths64& InPaths, double Delta,
                                   const EClipperJoinType Miter, double MiterLimit,
                                   const EClipperEndType EndType, double ArcTolerance)
{
	OutPaths = Clipper2Lib::InflatePaths(InPaths, Delta, static_cast<Clipper2Lib::JoinType>(Miter),
	                                     static_cast<Clipper2Lib::EndType>(EndType), MiterLimit, ArcTolerance);
}

void UClipper2Helper::InflatePaths(FClipperPathsD& OutPaths, const FClipperPathsD& InPaths, double Delta,
                                   const EClipperJoinType Miter, const double MiterLimit,
                                   const EClipperEndType EndType, double ArcTolerance, int32 Precision)
{
	OutPaths = Clipper2Lib::InflatePaths(InPaths, Delta, static_cast<Clipper2Lib::JoinType>(Miter),
	                                     static_cast<Clipper2Lib::EndType>(EndType), MiterLimit,
	                                     Precision, ArcTolerance);
}

void UClipper2Helper::InflatePaths(FClipperPathsD& OutPaths, const FClipperPathD& InPath, double Delta,
                                   const EClipperJoinType Miter, double MiterLimit, const EClipperEndType EndType,
                                   double ArcTolerance, int32 Precision)
{
	OutPaths = Clipper2Lib::InflatePaths(std::vector<FClipperPathD>{InPath}, Delta,
	                                     static_cast<Clipper2Lib::JoinType>(Miter),
	                                     static_cast<Clipper2Lib::EndType>(EndType), MiterLimit,
	                                     Precision, ArcTolerance);
}

template <typename RealType, typename OutputType>
TClipperPaths<OutputType> UClipper2Helper::ConvertPolygonsToPaths(
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
TClipperPath<OutputType> UClipper2Helper::ConvertPolygonToPath(
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
