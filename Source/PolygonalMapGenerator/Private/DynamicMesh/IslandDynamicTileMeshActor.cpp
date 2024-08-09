#include "DynamicMesh/IslandDynamicTileMeshActor.h"

#include "DynamicMeshActor.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"

DEFINE_LOG_CATEGORY(LogIslandDynamicActor)

AIslandDynamicTileMeshActor::AIslandDynamicTileMeshActor()
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent0"));
	RootComponent = SceneComponent;
}

void AIslandDynamicTileMeshActor::AsyncGenerateDynamicMesh(UIslandDynamicAssets* InAssets)
{
	if (!InAssets)
	{
		UE_LOG(LogIslandDynamicActor, Error, TEXT("Invalid Island Dynamic Assets"));
		return;
	}
	Assets = InAssets;
	CompletedTilesCount = 0;
	int32 TileAmount = Assets->TileInfo.Num();
	TileActors.SetNum(TileAmount);
	for (int32 Index = 0; Index < TileAmount; ++Index)
	{
		FDynamicTileInfo* InfoPtr = &Assets->TileInfo[Index];
		FGraphEventArray ApplyBuffersPrerequisites;
		ApplyBuffersPrerequisites.Emplace(InfoPtr->Task);
		FGraphEventRef ApplyBuffersTask = FFunctionGraphTask::CreateAndDispatchWhenReady([this, Index, InfoPtr]
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name =
				FName(FString::Printf(TEXT("IslandDynamicTileActor_%d_%d"), InfoPtr->TileRow, InfoPtr->TileCol));
			FVector2D PivotOffset = Assets->MapData->GetMapSize() * Pivot;
			FVector Location = FVector::Zero();
			Location.X = InfoPtr->TileCenter.X - PivotOffset.X;
			Location.Y = InfoPtr->TileCenter.Y - PivotOffset.Y;
			ADynamicMeshActor* TileActor = GetWorld()->SpawnActor<ADynamicMeshActor>(
				Location, FRotator::ZeroRotator, SpawnParameters);
			TileActors[Index] = TileActor;
			TileActor->AttachToActor(this, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));

			UDynamicMeshComponent* DynamicMeshComponent = TileActor->GetDynamicMeshComponent();
			UDynamicMesh* DynamicMesh = DynamicMeshComponent->GetDynamicMesh();
			FGeometryScriptIndexList TriangleIndices;

			UGeometryScriptLibrary_MeshBasicEditFunctions::AppendBuffersToMesh(
				DynamicMesh, InfoPtr->Buffers, TriangleIndices, 0, true
			);
			UGeometryScriptLibrary_MeshNormalsFunctions::SetPerVertexNormals(DynamicMesh);
			if (bGenerateCollision)
				UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
					DynamicMesh, DynamicMeshComponent, GenerateCollisionOptions);
		}, TStatId(), &ApplyBuffersPrerequisites, ENamedThreads::GameThread);

		FGraphEventArray SetMaterialsPrerequisites;
		SetMaterialsPrerequisites.Emplace(Assets->GenDistrictIDTextureTask);
		SetMaterialsPrerequisites.Emplace(ApplyBuffersTask);
		FFunctionGraphTask::CreateAndDispatchWhenReady([this, Index]
		{
			UDynamicMeshComponent* DynamicMeshComponent = TileActors[Index]->GetDynamicMeshComponent();
			if (IsValid(IslandMaterial))
			{
				UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(IslandMaterial, this);
				MaterialInstance->SetTextureParameterValue(DistrictIDTexture01ParamName,
				                                           Assets->GetDistrictIDTexture01());
				MaterialInstance->SetTextureParameterValue(DistrictIDTexture02ParamName,
				                                           Assets->GetDistrictIDTexture02());
				DynamicMeshComponent->SetMaterial(0, MaterialInstance);
			}
			CheckIfAllTilesAreCompleted();
		}, TStatId(), &SetMaterialsPrerequisites, ENamedThreads::GameThread);
	}
}

void AIslandDynamicTileMeshActor::CheckIfAllTilesAreCompleted()
{
	if (++CompletedTilesCount == Assets->GetTileAmount())
	{
		PostGenerateIsland(true);
	}
}

void AIslandDynamicTileMeshActor::PostGenerateIsland_Implementation(bool bSucceed)
{
}
