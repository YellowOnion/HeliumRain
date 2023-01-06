
#include "FlareDebrisField.h"
#include "../Flare.h"
#include "FlareGame.h"
#include "FlareSimulatedSector.h"

#include "Engine/StaticMeshActor.h"
#include "StaticMeshResources.h"

#define LOCTEXT_NAMESPACE "FlareDebrisField"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareDebrisField::UFlareDebrisField(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CurrentGenerationIndex = 0;
	// Load catalog
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UFlareAsteroidCatalog> RockCatalog;
		ConstructorHelpers::FObjectFinder<UFlareAsteroidCatalog> DebrisCatalog;
		FConstructorStatics()
			: RockCatalog(TEXT("/Game/ThirdParty/RocksDebris/RockDebrisCatalog.RockDebrisCatalog"))
			, DebrisCatalog(TEXT("/Game/Environment/Debris/MetalDebrisCatalog.MetalDebrisCatalog"))
		{}
	};
	static FConstructorStatics ConstructorStatics;

	// Push catalog data into storage
	RockCatalog = ConstructorStatics.RockCatalog.Object;
	DebrisCatalog = ConstructorStatics.DebrisCatalog.Object;
}

void UFlareDebrisField::Setup(AFlareGame* GameMode, UFlareSimulatedSector* Sector)
{
	FCHECK(Sector);
	FCHECK(GameMode);

	// Get debris field parameters
	Game = GameMode;
	const FFlareDebrisFieldInfo* DebrisFieldInfo = &Sector->GetDescription()->DebrisFieldInfo;
	UFlareAsteroidCatalog* DebrisFieldMeshes = DebrisFieldInfo->DebrisCatalog;
	TArray<AStaticMeshActor*> NewDebris;

	// Add debris
	if (DebrisFieldInfo && DebrisFieldMeshes)
	{
		float SectorScale = 5000 * 100;
		int32 DebrisCount = 100 * DebrisFieldInfo->DebrisFieldDensity;
		FLOGV("UFlareDebrisField::Setup : debris catalog is %s", *DebrisFieldMeshes->GetName());
		FLOGV("UFlareDebrisField::Setup : spawning debris field : gen %d, size = %d, icy = %d", CurrentGenerationIndex, DebrisCount, Sector->GetDescription()->IsIcy);

		for (int32 Index = 0; Index < DebrisCount; Index++)
		{
			int32 DebrisIndex = FMath::RandRange(0, DebrisFieldMeshes->Asteroids.Num() - 1);

			float MinSize = DebrisFieldInfo->MinDebrisSize;
			float MaxSize = DebrisFieldInfo->MaxDebrisSize;
			float Size = FMath::FRandRange(MinSize, MaxSize);

			FName Name = FName(*(FString::Printf(TEXT("DebrisGen%dIndex%d"), CurrentGenerationIndex, Index)));
			NewDebris.Add(AddDebris(Sector, DebrisFieldMeshes->Asteroids[DebrisIndex], Size, SectorScale, Name));
		}
	}
	else
	{
		FLOG("UFlareDebrisField::Setup : debris catalog not available, skipping");
	}

	// Remember new debris
	DebrisField.Empty();

	for (int DebrisIndex = 0; DebrisIndex < NewDebris.Num(); DebrisIndex++)
	{
		if (NewDebris[DebrisIndex])
		{
			DebrisField.Add(NewDebris[DebrisIndex]);
		}
	}

	CurrentGenerationIndex++;
}

void UFlareDebrisField::Reset()
{
	FLOGV("UFlareDebrisField::Reset : clearing debris field, size = %d", DebrisField.Num());
	for (int i = 0; i < DebrisField.Num(); i++)
	{
//		Game->GetWorld()->DestroyActor(DebrisField[i]);
		Game->GetCacheSystem()->StoreCachedDebris(DebrisField[i]);
		DebrisField[i]->SetActorEnableCollision(false);
		DebrisField[i]->SetActorHiddenInGame(true);

		UStaticMeshComponent* DebrisComponent = DebrisField[i]->GetStaticMeshComponent();
		if (DebrisComponent)
		{
			DebrisComponent->SetSimulatePhysics(false);
		}
	}
	DebrisField.Empty();
}

void UFlareDebrisField::SetWorldPause(bool Pause)
{
	for (int i = 0; i < DebrisField.Num(); i++)
	{
		DebrisField[i]->SetActorHiddenInGame(Pause);
		DebrisField[i]->CustomTimeDilation = (Pause ? 0.f : 1.0);
		Cast<UPrimitiveComponent>(DebrisField[i]->GetRootComponent())->SetSimulatePhysics(!Pause);
	}
}

void UFlareDebrisField::CreateDebris(UFlareSimulatedSector* Sector, FVector Location, int32 Quantity, float MinSize, float MaxSize, bool IsMetal)
{
	if (!Sector || !Game)
	{
		return;
	}

	bool IsActiveSector = Game->GetActiveSector() != nullptr;
	if (IsActiveSector)
	{
		UFlareSimulatedSector* CurrentActiveSector = Game->GetActiveSector()->GetSimulatedSector();
		if (CurrentActiveSector != Sector)
		{
			return;
		}
	}

	UFlareAsteroidCatalog* DebrisFieldMeshes;
	if (IsMetal)
	{
		DebrisFieldMeshes = DebrisCatalog;
	}
	else
	{
		DebrisFieldMeshes = RockCatalog;
	}

	if (!MinSize || !MaxSize)
	{
		const FFlareDebrisFieldInfo* DebrisFieldInfo = &Sector->GetDescription()->DebrisFieldInfo;
		MinSize = DebrisFieldInfo->MinDebrisSize;
		MaxSize = DebrisFieldInfo->MaxDebrisSize;
	}

	while (Quantity > 0)
	{
		FRotator Rotation = FRotator(FMath::FRandRange(0, 360), FMath::FRandRange(0, 360), FMath::FRandRange(0, 360));
		float Size = FMath::FRandRange(MinSize, MaxSize);
		int32 DebrisIndex = FMath::RandRange(0, DebrisFieldMeshes->Asteroids.Num() - 1);

		int32 Index = DebrisField.Num();
		FName Name = FName(*(FString::Printf(TEXT("DebrisGen%dIndex%d"), CurrentGenerationIndex, Index)));

		FActorSpawnParameters Params;
		Params.Name = Name;
		Params.Owner = Game;
		Params.bNoFail = false;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

		AStaticMeshActor* DebrisMesh = SpawnDebris(Sector, DebrisFieldMeshes->Asteroids[DebrisIndex], Location, Rotation, Size, Params);
		if (DebrisMesh)
		{
			DebrisField.Add(DebrisMesh);
		}
		Quantity--;
	}
}


/*----------------------------------------------------
	Internals
----------------------------------------------------*/
AStaticMeshActor* UFlareDebrisField::AddDebris(UFlareSimulatedSector* Sector, UStaticMesh* Mesh, float Size, float SectorScale, FName Name)
{
	FActorSpawnParameters Params;
	Params.Name = Name;
	Params.Owner = Game;
	Params.bNoFail = false;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;

	// Compute size, location and rotation
	FVector Location = FMath::VRand() * SectorScale * FMath::FRandRange(0.2, 1.0);
	FRotator Rotation = FRotator(FMath::FRandRange(0, 360), FMath::FRandRange(0, 360), FMath::FRandRange(0, 360));

	AStaticMeshActor* DebrisMesh = SpawnDebris(Sector, Mesh, Location, Rotation, Size, Params);
	return DebrisMesh;
}

AStaticMeshActor* UFlareDebrisField::SpawnDebris(UFlareSimulatedSector* Sector,UStaticMesh* Mesh, FVector Location, FRotator Rotation, float Size, FActorSpawnParameters Params)
{
	// Spawn
	AStaticMeshActor* DebrisMesh = Game->GetCacheSystem()->RetrieveCachedDebris();
	bool FromPool = false;
	if (IsValid(DebrisMesh))
	{
		DebrisMesh->SetActorLocationAndRotation(Location, Rotation);
		DebrisMesh->SetActorHiddenInGame(false);
		FromPool = true;
	}
	else
	{
		DebrisMesh = Game->GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, Params);
	}

	if (DebrisMesh)
	{
		DebrisMesh->SetMobility(EComponentMobility::Movable);
		DebrisMesh->SetActorScale3D(Size * FVector(1, 1, 1));
		DebrisMesh->SetActorEnableCollision(true);

		// Setup
		UStaticMeshComponent* DebrisComponent = DebrisMesh->GetStaticMeshComponent();
		if (DebrisComponent)
		{
			DebrisComponent->SetStaticMesh(Mesh);
			DebrisComponent->SetSimulatePhysics(true);
			DebrisComponent->SetCollisionProfileName("BlockAllDynamic");

			int32 LODCOunt = DebrisComponent->GetStaticMesh()->GetNumLODs();

			if (FromPool)
			{
				DebrisComponent->SetMaterial(0, NULL);
			}

			// Set material
			UMaterialInstanceDynamic* DebrisMaterial = UMaterialInstanceDynamic::Create(DebrisComponent->GetMaterial(0), DebrisComponent->GetWorld());
			if (DebrisMaterial)
			{
				for (int32 i = 0; i < LODCOunt; i++)
				{
					DebrisComponent->SetMaterial(i, DebrisMaterial);
				}
				DebrisMaterial->SetScalarParameterValue("IceMask", Sector->GetDescription()->IsIcy);
			}
			else
			{
				FLOG("UFlareDebrisField::CreateDebris : failed to set material (no material or mesh)")
			}
		}
	}
	else
	{
		FLOG("UFlareDebrisField::CreateDebris : failed to spawn debris")
	}
	return DebrisMesh;
}

#undef LOCTEXT_NAMESPACE