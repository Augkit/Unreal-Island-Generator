#include "DynamicMesh/IslandDynamicTileMeshActor.h"
#include "DynamicMeshActor.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"

DEFINE_LOG_CATEGORY(LogIslandDynamicActor)

AIslandDynamicTileMeshActor::AIslandDynamicTileMeshActor()
{
	PrimaryActorTick.bCanEverTick = true;
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
	SpawnedTileActorsCount = 0;
	int32 TileAmount = Assets->GetTileAmount();
	TileActors.SetNum(TileAmount);
	for (int32 Index = 0; Index < TileAmount; ++Index)
	{
		FGraphEventArray ApplyBuffersPrerequisites;
		ApplyBuffersPrerequisites.Emplace(Assets->TileInfo[Index].Task);
		FFunctionGraphTask::CreateAndDispatchWhenReady([this, Index]
		{
			TileToSpawnQueue.Enqueue(Index);
		}, TStatId(), &ApplyBuffersPrerequisites);
	}
}

void AIslandDynamicTileMeshActor::CheckIfAllTilesAreCompleted()
{
	if (++CompletedTilesCount == Assets->GetTileAmount())
	{
		PostGenerateIsland(true);
	}
}

void AIslandDynamicTileMeshActor::Tick(float DeltaSeconds)
{
	const FTimespan MaxTick(ETimespan::TicksPerSecond * MaxSpawnTileTickTime);
	FDateTime TickStartTime = FDateTime::Now();
	Super::Tick(DeltaSeconds);
	while (SpawnedTileActorsCount < Assets->GetTileAmount())
	{
		if (int32 TaskIndex; TileToSpawnQueue.Dequeue(TaskIndex))
		{
			++SpawnedTileActorsCount;
			FDynamicTileInfo TileInfo = Assets->TileInfo[TaskIndex];
			TRACE_CPUPROFILER_EVENT_SCOPE(AsyncGenerateDynamicMesh::SpawnTileActor);
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name =
				FName(FString::Printf(TEXT("IslandDynamicTileActor_%d_%d"), TileInfo.TileRow, TileInfo.TileCol));
			FVector2D Offset = Assets->MapData->GetMapSize() * Pivot;
			FVector Location = FVector::Zero();
			Location.X = TileInfo.TileCenter.X - Offset.X;
			Location.Y = TileInfo.TileCenter.Y - Offset.Y;
			ADynamicMeshActor* TileActor = GetWorld()->SpawnActor<ADynamicMeshActor>(
				Location, FRotator::ZeroRotator, SpawnParameters);
			TileActors[TaskIndex] = TileActor;
			TileActor->AttachToActor(this, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));

			UDynamicMeshComponent* DynamicMeshComponent = TileActor->GetDynamicMeshComponent();
			UDynamicMesh* DynamicMesh = DynamicMeshComponent->GetDynamicMesh();
			FGeometryScriptIndexList TriangleIndices;

			UGeometryScriptLibrary_MeshBasicEditFunctions::AppendBuffersToMesh(
				DynamicMesh, TileInfo.Buffers, TriangleIndices, 0, true
			);
			UGeometryScriptLibrary_MeshNormalsFunctions::SetPerVertexNormals(DynamicMesh);
			if (bGenerateCollision)
			{
				UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
					DynamicMesh, DynamicMeshComponent, GenerateCollisionOptions);
			}
			FGraphEventArray SetMaterialsPrerequisites;
			SetMaterialsPrerequisites.Emplace(Assets->GenDistrictIDTextureTask);
			FFunctionGraphTask::CreateAndDispatchWhenReady([this, TileActor]
			{
				UDynamicMeshComponent* DynamicMeshComponent = TileActor->GetDynamicMeshComponent();
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
		if (FDateTime::Now() - TickStartTime > MaxTick)
		{
			break;
		}
	}
}

void AIslandDynamicTileMeshActor::PostGenerateIsland_Implementation(bool bSucceed)
{
}
