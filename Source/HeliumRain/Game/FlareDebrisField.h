#pragma once

#include "Object.h"
#include "FlareDebrisField.generated.h"


class AFlareGame;
class UFlareSimulatedSector;

class AStaticMeshActor;


UCLASS()
class HELIUMRAIN_API UFlareDebrisField : public UObject
{
    GENERATED_UCLASS_BODY()

public:

    /*----------------------------------------------------
        Public interface
    ----------------------------------------------------*/

	/** Load the debris field from a sector info */
	void Setup(AFlareGame* GameMode, UFlareSimulatedSector* Sector);

	/** Cleanup the debris field */
	void Reset();

	/** Toggle the game pause */
	void SetWorldPause(bool Pause);

	void CreateDebris(UFlareSimulatedSector* Sector, FVector Location, int32 Quantity = 1, float MinSize = 3, float MaxSize = 7, bool IsMetal = true);

	UFlareAsteroidCatalog* GetRockCatalog() const
	{
		return RockCatalog;
	}

	UFlareAsteroidCatalog* GetDebrisCatalog() const
	{
		return DebrisCatalog;
	}

private:

	/*----------------------------------------------------
		Internals
	----------------------------------------------------*/

	/** Add debris */
	AStaticMeshActor* AddDebris(UFlareSimulatedSector* Sector, UStaticMesh* Mesh, float Size, float SectorScale, FName Name);
	AStaticMeshActor* SpawnDebris(UFlareSimulatedSector* Sector, UStaticMesh* Mesh, FVector Location, FRotator Rotation, float Size, FActorSpawnParameters Params);

protected:

    /*----------------------------------------------------
        Protected data
    ----------------------------------------------------*/

	/** Debris field */
	UPROPERTY()
	TArray<AStaticMeshActor*>                  DebrisField;
	
	/** Game reference */
	UPROPERTY()
	AFlareGame*                                Game;

	// Data
	int32                                      CurrentGenerationIndex;

	UPROPERTY()
	UFlareAsteroidCatalog*                     RockCatalog;

	UPROPERTY()
	UFlareAsteroidCatalog*                     DebrisCatalog;

};
