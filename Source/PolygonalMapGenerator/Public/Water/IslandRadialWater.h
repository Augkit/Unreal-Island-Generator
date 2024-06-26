/*
* From http://www.redblobgames.com/maps/mapgen2/
* Original work copyright 2017 Red Blob Games <redblobgames@gmail.com>
* Unreal Engine 4 implementation copyright 2018 Jay Stevens <jaystevens42@gmail.com>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include "CoreMinimal.h"
#include "Water/IslandWater.h"
#include "IslandRadialWater.generated.h"

/**
 * 
 */
UCLASS()
class POLYGONALMAPGENERATOR_API UIslandRadialWater : public UIslandWater
{
	GENERATED_BODY()

public:
	// The number of sine waves which form bumps along the island.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bumps")
	int32 Bumps;
	// The start angle for the sin function, in radians.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angle")
	bool bRandomStartAngle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angle", meta = ( EditCondition = "!bRandomStartAngle" ))
	float StartAngle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angle")
	float AngleOffset;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angle")
	float MinAngle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angle")
	float LandScale;

public:
	UIslandRadialWater();

protected:
	mutable float RandomStartAngle;
	virtual void InitializeWater_Implementation(TArray<bool>& r_water, UTriangleDualMesh* Mesh, FRandomStream& Rng) const override;
	virtual bool IsPointLand_Implementation(FPointIndex Point, UTriangleDualMesh* Mesh, const FVector2D& HalfMeshSize, const FVector2D& Offset, const FIslandShape& Shape) const override;
};
