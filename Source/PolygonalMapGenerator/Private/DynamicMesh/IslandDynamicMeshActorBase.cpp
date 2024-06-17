// Fill out your copyright notice in the Description page of Project Settings.


#include "DynamicMesh/IslandDynamicMeshActorBase.h"

struct FCoastlinePolygon;

UIslandMapData* AIslandDynamicMeshActorBase::SetMapData(UIslandMapData* InMapData)
{
	MapData = InMapData;
	return MapData;
}

UIslandMapData* AIslandDynamicMeshActorBase::GetMapData()
{
	return MapData;
}

bool AIslandDynamicMeshActorBase::GenerateIsland(UIslandMapData* InMapData)
{
	if (InMapData != nullptr)
		SetMapData(InMapData);
	if (MapData == nullptr)
	{
		PostGenerateIsland(false);
		return false;
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_GenerateIslandTexture)
		GenerateIslandTexture();
	}
	UDynamicMesh* DynamicMesh = DynamicMeshComponent->GetDynamicMesh();
	{
		SCOPE_CYCLE_COUNTER(STAT_GenerateDynamicMesh)
		GenerateIslandMesh(DynamicMesh);
	}
	if (bGenerateCollision)
		UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
			DynamicMesh, DynamicMeshComponent, GenerateCollisionOptions);
	if (IsValid(IslandMaterial))
	{
		UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(IslandMaterial, this);
		SetMaterialParameters(MaterialInstance);
		GetDynamicMeshComponent()->SetMaterial(0, MaterialInstance);
	}
	PostGenerateIsland(true);
	return true;
}

void AIslandDynamicMeshActorBase::PostGenerateIsland_Implementation(bool bSucceed)
{
	// Empty
}

void AIslandDynamicMeshActorBase::GenerateIslandTexture()
{
	// Empty
}

void AIslandDynamicMeshActorBase::GenerateIslandMesh(UDynamicMesh* DynamicMesh)
{
	// Empty
}

void AIslandDynamicMeshActorBase::SetMaterialParameters(UMaterialInstanceDynamic* MaterialInstance)
{
	// Empty
}
