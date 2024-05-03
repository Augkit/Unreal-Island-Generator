#include "PolyPartitionHelper.h"

#include "PolyPartitionLib.h"

TPPLPartition UPolyPartitionHelper::PolyPartition = TPPLPartition();

void UPolyPartitionHelper::Triangulate(const TArray<FVector2D>& Points, const TArray<int32>& PointId,
                                       TArray<FPolyTriangle2D>& Triangles)
{
	TPPLPoly Poly = MakePoly(Points, PointId);
	TPPLPolyList PolyTriangles;
	PolyPartition.Triangulate_EC(&Poly, &PolyTriangles);
	for (TPPLPolyList::iterator TriIt = PolyTriangles.begin(); TriIt != PolyTriangles.end(); ++TriIt)
	{
		const TPPLPoly& Tri = *TriIt;
		const TPPLPoint& V0 = Tri.GetPoint(0);
		const TPPLPoint& V1 = Tri.GetPoint(1);
		const TPPLPoint& V2 = Tri.GetPoint(2);
		Triangles.Add(FPolyTriangle2D(V0, V0.id, V1, V1.id, V2, V2.id));
	}
}

void UPolyPartitionHelper::TriangulateWithHole(const TArray<FVector2D>& Points, const TArray<int32>& PointId,
                                               const TArray<FVector2D>& HolePoints, const TArray<int32>& HolePointId,
                                               TArray<FPolyTriangle2D>& Triangles)
{
	TPPLPolyList Polys;
	Polys.push_back(MakePoly(Points, PointId));
	Polys.push_back(MakePoly(HolePoints, HolePointId, true));
	TPPLPolyList PolyTriangles;
	PolyPartition.Triangulate_EC(&Polys, &PolyTriangles);
	for (TPPLPolyList::iterator TriIt = PolyTriangles.begin(); TriIt != PolyTriangles.end(); ++TriIt)
	{
		const TPPLPoly& Tri = *TriIt;
		const TPPLPoint& V0 = Tri.GetPoint(0);
		const TPPLPoint& V1 = Tri.GetPoint(1);
		const TPPLPoint& V2 = Tri.GetPoint(2);
		Triangles.Add(FPolyTriangle2D(V0, V0.id, V1, V1.id, V2, V2.id));
	}
}

TPPLPoly UPolyPartitionHelper::MakePoly(const TArray<FVector2D>& Points, const TArray<int32>& PointId, bool bIsHole)
{
	TPPLPoly Poly;
	const int32 PointNum = Points.Num();
	Poly.Init(PointNum);
	Poly.SetHole(bIsHole);
	for (int32 Index = 0; Index < PointNum; ++Index)
	{
		Poly[Index] = Points[Index];
		Poly[Index].id = PointId.IsValidIndex(Index) ? PointId[Index] : Index;
	}
	Poly.SetOrientation(bIsHole ? TPPL_ORIENTATION_CW : TPPL_ORIENTATION_CCW);
	return Poly;
}
