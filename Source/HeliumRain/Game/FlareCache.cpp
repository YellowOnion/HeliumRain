
#include "FlareCache.h"
#include "../Flare.h"
#include "FlareGame.h"

#define LOCTEXT_NAMESPACE "FlareCacheSystem"

/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

//TODO: perhaps use TMap or multi-layered Array and more generic get/store for each type
//Key would probably be a string

UFlareCacheSystem::UFlareCacheSystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
void UFlareCacheSystem::InitialSetup(AFlareGame* GameMode)
{
	FCHECK(GameMode);
	Game = GameMode;

	CachedShells.Empty();
	CachedBombs.Empty();
	CachedAsteroids.Empty();
	CachedDebris.Empty();
}

void UFlareCacheSystem::StoreCachedShell(AFlareShell* Shell)
{
	if (Shell)
	{
		CachedShells.Add(Shell);
	}
}

AFlareShell* UFlareCacheSystem::RetrieveCachedShell()
{
	if (CachedShells.Num() > 0)
	{
		AFlareShell* Shell = CachedShells.Pop();
		return Shell;
	}
	return NULL;
}

void UFlareCacheSystem::StoreCachedBomb(AFlareBomb* Bomb)
{
	if (Bomb)
	{
		CachedBombs.Add(Bomb);
	}
}

AFlareBomb* UFlareCacheSystem::RetrieveCachedBomb()
{
	if (CachedBombs.Num() > 0)
	{
		AFlareBomb* Bomb = CachedBombs.Pop();
		return Bomb;
	}
	return NULL;
}

void UFlareCacheSystem::StoreCachedAsteroid(AFlareAsteroid* Asteroid)
{
	if (Asteroid)
	{
		CachedAsteroids.Add(Asteroid);
	}
}

AFlareAsteroid* UFlareCacheSystem::RetrieveCachedAsteroid()
{
	if (CachedAsteroids.Num() > 0)
	{
		AFlareAsteroid* Asteroid = CachedAsteroids.Pop();
		return Asteroid;
	}
	return NULL;
}

void UFlareCacheSystem::StoreCachedDebris(AStaticMeshActor* Debris)
{
	if (Debris)
	{
		CachedDebris.Add(Debris);
	}
}

AStaticMeshActor* UFlareCacheSystem::RetrieveCachedDebris()
{
	if (CachedDebris.Num() > 0)
	{
		AStaticMeshActor* Debris = CachedDebris.Pop();
		return Debris;
	}
	return NULL;
}

#undef LOCTEXT_NAMESPACE