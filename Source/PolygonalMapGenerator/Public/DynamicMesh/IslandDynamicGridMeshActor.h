#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/IslandDynamicMeshActor.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "IslandDynamicGridMeshActor.generated.h"

UCLASS(BlueprintType, Blueprintable)
class POLYGONALMAPGENERATOR_API AIslandDynamicGridMeshActor : public AIslandDynamicMeshActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 GridDivisions = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 GridResolution = 100;
	
	virtual void GenerateIslandMesh(UDynamicMesh* DynamicMesh, const FTransform& Transform) override;

protected:
	UPROPERTY()
	TArray<FGeometryScriptSimpleMeshBuffers> GridMeshBuffers;

	UPROPERTY()
	FGeometryScriptSimpleMeshBuffers GridMeshBufferGroup;

	virtual void CalcGridMeshBuffer(const int32 GridIndex, const FTransform Transform);
};
