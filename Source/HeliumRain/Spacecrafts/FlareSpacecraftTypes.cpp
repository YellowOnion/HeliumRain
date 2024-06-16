
#include "FlareSpacecraftTypes.h"
#include "../Flare.h"
#include "../Data/FlareFactoryCatalogEntry.h"

DECLARE_CYCLE_STAT(TEXT("SpacecraftHelper GetIntersectionPosition"), STAT_SpacecraftHelper_GetIntersectionPosition, STATGROUP_Flare);

/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareSpacecraftTypes::UFlareSpacecraftTypes(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

float SpacecraftHelper::GetIntersectionPosition(FVector TargetLocation, FVector TargetVelocity, FVector SourceLocation, FVector SourceVelocity, float ProjectileSpeed, float PredictionDelay, FVector* ResultPosition)
{

	SCOPE_CYCLE_COUNTER(STAT_SpacecraftHelper_GetIntersectionPosition);


	/*FLOGV("GetIntersectionPosition at %f s",PredictionDelay);
	FLOGV("  TargetLocation=%s",*TargetLocation.ToString());
	FLOGV("  TargetVelocity=%s",*TargetVelocity.ToString());
	FLOGV("  SourceLocation=%s",*SourceLocation.ToString());
	FLOGV("  SourceVelocity=%s",*SourceVelocity.ToString());
	FLOGV("  ProjectileSpeed=%f",ProjectileSpeed);*/


	FVector RelativeTargetVelocity = TargetVelocity - SourceVelocity;

	// Relative Target Speed
	FVector PredictedTargetLocation = TargetLocation + RelativeTargetVelocity * PredictionDelay;
	FVector BulletLocation = SourceLocation;

	// Find the relative speed in the axis of target
	FVector TargetDirection = (PredictedTargetLocation - BulletLocation).GetUnsafeNormal();

	float EffectiveProjectileSpeed = ProjectileSpeed;

	float Divisor = FMath::Square(EffectiveProjectileSpeed) - RelativeTargetVelocity.SizeSquared();


	/*FLOGV("  PredictedTargetLocation=%s",*PredictedTargetLocation.ToString());
	FLOGV("  BulletLocation=%s",*BulletLocation.ToString());
	FLOGV("  TargetDirection=%s",*TargetDirection.ToString());


	FLOGV("  EffectiveProjectileSpeed=%f",EffectiveProjectileSpeed);
	FLOGV("  Divisor=%f",Divisor);*/

	if (EffectiveProjectileSpeed < 0 || FMath::IsNearlyZero(Divisor))
	{
		// Intersect at an infinite time
		return -1;
	}

	float A = -1;
	float B = 2 * (RelativeTargetVelocity.X * (PredictedTargetLocation.X - BulletLocation.X) + RelativeTargetVelocity.Y * (PredictedTargetLocation.Y - BulletLocation.Y) + RelativeTargetVelocity.Z * (PredictedTargetLocation.Z - BulletLocation.Z)) / Divisor;
	float C = (PredictedTargetLocation - BulletLocation).SizeSquared() / Divisor;

	float Delta = FMath::Square(B) - 4 * A * C;

	float InterceptTime1 = (- B - FMath::Sqrt(Delta)) / (2 * A);
	float InterceptTime2 = (- B + FMath::Sqrt(Delta)) / (2 * A);

	float InterceptTime;
	if (InterceptTime1 > 0 && InterceptTime2 > 0)
	{
		InterceptTime = FMath::Min(InterceptTime1, InterceptTime2);
	}
	else
	{
		InterceptTime = FMath::Max(InterceptTime1, InterceptTime2);
	}

	if (InterceptTime > 0)
	{
		FVector InterceptLocation = PredictedTargetLocation + RelativeTargetVelocity * InterceptTime;
		InterceptLocation += SourceVelocity * PredictionDelay;
		*ResultPosition = InterceptLocation;
	}
	return InterceptTime;
}

EFlareDamage::Type SpacecraftHelper::GetWeaponDamageType(EFlareShellDamageType::Type ShellDamageType)
{
	switch(ShellDamageType) {
	case EFlareShellDamageType::HighExplosive:
		return EFlareDamage::DAM_HighExplosive;
	case EFlareShellDamageType::ArmorPiercing:
	case EFlareShellDamageType::LightSalvage:
	case EFlareShellDamageType::HeavySalvage:
		return EFlareDamage::DAM_ArmorPiercing;
	case EFlareShellDamageType::HEAT:
		return EFlareDamage::DAM_HEAT;
	default:
		return EFlareDamage::DAM_None;
	};
}

int32 FFlareSpacecraftDescription::GetCapacity() const
{
	return CargoBayCapacity * CargoBayCount;
}

bool FFlareSpacecraftDescription::IsAnUncapturable() const
{
	return IsUncapturable;
}

bool FFlareSpacecraftDescription::IsStation() const
{
	return OrbitalEngineCount == 0 || IsAStation;
}

bool FFlareSpacecraftDescription::IsImmobileStation() const
{
	return OrbitalEngineCount == 0;
}

bool FFlareSpacecraftDescription::IsShipyard() const
{
	if (IsStation())
	{
		for (int32 FactoryIndex = 0; FactoryIndex < Factories.Num(); FactoryIndex++)
		{
			FFlareFactoryDescription* FactoryDescription = &Factories[FactoryIndex]->Data;

			if (FactoryDescription->IsShipyard())
			{
				return true;
			}
		}
	}
	return false;
}

bool FFlareSpacecraftDescription::IsMilitary() const
{
	return (!IsNotMilitary && (GunSlots.Num() > 0 || TurretSlots.Num() > 0));
}

bool FFlareSpacecraftDescription::IsMilitaryArmed() const
{
	return GunSlots.Num() > 0 || TurretSlots.Num() > 0;
}

bool FFlareSpacecraftDescription::CheckIsNotMilitary() const
{
	return IsNotMilitary;
}

bool FFlareSpacecraftDescription::IsResearch() const
{
	for (int32 FactoryIndex = 0; FactoryIndex < Factories.Num(); FactoryIndex++)
	{
		FFlareFactoryDescription* FactoryDescription = &Factories[FactoryIndex]->Data;

		if(FactoryDescription->IsResearch())
		{
			return true;
		}
	}
	return false;
}

bool FFlareSpacecraftDescription::IsTelescope() const
{
	for (int32 FactoryIndex = 0; FactoryIndex < Factories.Num(); FactoryIndex++)
	{
		FFlareFactoryDescription* FactoryDescription = &Factories[FactoryIndex]->Data;

		if (FactoryDescription->IsTelescope())
		{
			return true;
		}
	}
	return false;
}