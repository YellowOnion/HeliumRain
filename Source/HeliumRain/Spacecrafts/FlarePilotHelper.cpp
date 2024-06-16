
#include "FlarePilotHelper.h"
#include "../Flare.h"
#include "../Game/FlareCompany.h"
#include "../Game/FlareSector.h"
#include "../Game/FlareGame.h"
#include "../Game/FlareCollider.h"
#include "FlareRCS.h"
#include "FlareOrbitalEngine.h"
#include "FlareWeapon.h"
#include "FlareShipPilot.h"
#include "../Quests/FlareMeteorite.h"

#include "Components/StaticMeshComponent.h"

DECLARE_CYCLE_STAT(TEXT("PilotHelper CheckFriendlyFire"), STAT_PilotHelper_CheckFriendlyFire, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("PilotHelper Anticollision"), STAT_PilotHelper_AnticollisionCorrection, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("PilotHelper Anticollision Avoidance"), STAT_PilotHelper_AnticollisionCorrection_Avoidance, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("PilotHelper GetBestTarget"), STAT_PilotHelper_GetBestTarget, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("PilotHelper GetBestTargetComponent"), STAT_PilotHelper_GetBestTargetComponent, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("PilotHelper CheckRelativeDangerosity"), STAT_PilotHelper_CheckRelativeDangerosity, STATGROUP_Flare);

bool PilotHelper::CheckFriendlyFire(UFlareSector* Sector, UFlareCompany* MyCompany, FVector FireBaseLocation, FVector FireBaseVelocity , float AmmoVelocity, FVector FireAxis, float MaxDelay, float AimRadius)
{
	SCOPE_CYCLE_COUNTER(STAT_PilotHelper_CheckFriendlyFire);
	//FLOG("CheckFriendlyFire");
	for (int32 SpacecraftIndex = 0; SpacecraftIndex < Sector->GetSpacecrafts().Num(); SpacecraftIndex++)
	{
		AFlareSpacecraft* SpacecraftCandidate = Sector->GetSpacecrafts()[SpacecraftIndex];

		if (SpacecraftCandidate)
		{
			if (SpacecraftCandidate->IsHostile(MyCompany))
			{
				//FLOG("  hostile, skip");
				continue;
			}

			// Compute target Axis for each gun
			FVector AmmoIntersectionLocation;
			float AmmoIntersectionTime = SpacecraftHelper::GetIntersectionPosition(SpacecraftCandidate->GetActorLocation(), SpacecraftCandidate->Airframe->GetPhysicsLinearVelocity(), FireBaseLocation, FireBaseVelocity , AmmoVelocity, 0, &AmmoIntersectionLocation);
			if (AmmoIntersectionTime < 0 || AmmoIntersectionTime > MaxDelay)
			{
				// No ammo intersection, or too far, don't alert
				//FLOGV("  Too far (%f > %f)", AmmoIntersectionTime , MaxDelay);
				continue;
			}

			float TargetSize = SpacecraftCandidate->GetMeshScale() / 100.f + AimRadius * 2; // Radius in meters
			FVector DeltaLocation = (SpacecraftCandidate->GetActorLocation()-FireBaseLocation) / 100.f;
			float Distance = DeltaLocation.Size();

			FVector FireTargetAxis = (AmmoIntersectionLocation - FireBaseLocation - AmmoIntersectionTime * FireBaseVelocity).GetUnsafeNormal();

			float AngularPrecisionDot = FVector::DotProduct(FireTargetAxis, FireAxis);
			float AngularPrecision = FMath::Acos(AngularPrecisionDot);
			float AngularSize = FMath::Atan(TargetSize / Distance);



			/*FLOGV("  TargetSize %f", TargetSize);
			FLOGV("  Distance %f", Distance);
			FLOGV("  AngularPrecision %f", AngularPrecision);
			FLOGV("  AngularSize %f", AngularSize);*/
			if (AngularPrecision < AngularSize)
			{
				//FLOG("  Dangerous");
				// Dangerous !
				return true;
			}
		}
	}
	return false;

}

bool PilotHelper::FindMostDangerousCollision(AActor*& MostDangerousCandidateActor,
											 FVector& MostDangerousLocation,
											 float& MostDangerousTimeToHit,
											 float& MostDangerousInterceptDepth,
											 AFlareSpacecraft* Ship, AnticollisionConfig IgnoreConfig, float SpeedLimit)
{
	SCOPE_CYCLE_COUNTER(STAT_PilotHelper_AnticollisionCorrection);

	typedef TPair<AActor*, FVector> TFlareCollisionCandidate;

	UFlareSector* ActiveSector = Ship->GetGame()->GetActiveSector();
	TArray<TFlareCollisionCandidate> Candidates;
	TFlareCollisionCandidate Candidate;

	// Select dangerous ships
	for (auto SpacecraftCandidate : ActiveSector->GetSpacecrafts())
	{
		if (SpacecraftCandidate != Ship
		 && SpacecraftCandidate != IgnoreConfig.SpacecraftToIgnore
		 && !(IgnoreConfig.IgnoreAllStations && SpacecraftCandidate->IsStation())
		 && !Ship->GetDockingSystem()->IsGrantedShip(SpacecraftCandidate)
		 && !Ship->GetDockingSystem()->IsDockedShip(SpacecraftCandidate)
		 && !(Ship->GetSize() == EFlarePartSize::L
			  && SpacecraftCandidate->GetSize() == EFlarePartSize::S
			  && IsTargetDangerous(PilotTarget(SpacecraftCandidate))
			  && SpacecraftCandidate->IsHostile(Ship->GetCompany()))
		&& !(IgnoreConfig.SpacecraftToIgnore && IgnoreConfig.SpacecraftToIgnore->IsStation() && IgnoreConfig.SpacecraftToIgnore->GetParent()->IsComplexElement() && SpacecraftCandidate->GetParent() == IgnoreConfig.SpacecraftToIgnore->GetParent()->GetComplexMaster())
		&& !(IgnoreConfig.SpacecraftToIgnore && IgnoreConfig.SpacecraftToIgnore->IsStation() && IgnoreConfig.SpacecraftToIgnore->GetParent()->IsComplex() && SpacecraftCandidate->GetParent()->GetComplexMaster() == IgnoreConfig.SpacecraftToIgnore->GetParent())
		)
		{
			Candidate.Key = SpacecraftCandidate;
			Candidate.Value = SpacecraftCandidate->Airframe->GetPhysicsLinearVelocity();
			Candidates.Add(Candidate);
		}
	}

	// Select dangerous asteroids
//	Candidates.Reserve(Candidates.Num() + ActiveSector->GetAsteroids().Num());
	for (auto AsteroidCandidate : ActiveSector->GetAsteroids())
	{
		Candidate.Key = AsteroidCandidate;
		Candidate.Value = AsteroidCandidate->GetAsteroidComponent()->GetPhysicsLinearVelocity();
		Candidates.Add(Candidate);
	}

	// Select dangerous meteorites
//	Candidates.Reserve(Candidates.Num() + ActiveSector->GetMeteorites().Num());
	for (auto MeteoriteCandidate : ActiveSector->GetMeteorites())
	{
		if(!MeteoriteCandidate->IsBroken())
		{
			Candidate.Key = MeteoriteCandidate;
			Candidate.Value = MeteoriteCandidate->GetMeteoriteComponent()->GetPhysicsLinearVelocity();
			Candidates.Add(Candidate);
		}
	}

	// Select dangerous colliders
	TArray<AActor*> ColliderActorList;
	UGameplayStatics::GetAllActorsOfClass(Ship->GetWorld(), AFlareCollider::StaticClass(), ColliderActorList);
//	Candidates.Reserve(Candidates.Num() + ColliderActorList.Num());
	for (auto ColliderCandidate : ColliderActorList)
	{
		Candidate.Key = Cast<AFlareCollider>(ColliderCandidate);
		Candidate.Value = FVector::ZeroVector;
		Candidates.Add(Candidate);
	}

	// No candidate found, return
	if (Candidates.Num() == 0)
	{
		return false;
	}

	// Input data for danger processing
	FBox ShipBox = Ship->GetComponentsBoundingBox();
//	FBox ShipBox = Ship->GetMeshBox();
	FVector CurrentVelocity = Ship->GetLinearVelocity() * 100;
	FVector CurrentLocation = (ShipBox.Max + ShipBox.Min) / 2.0;
	float CurrentSize = FMath::Max(ShipBox.GetExtent().Size(), 1.0f);
	float MaxRelevanceDistance = 200 * CurrentSize;

	// Output data
	MostDangerousCandidateActor = NULL;

	// Process all candidates
	for (auto SelectedCandidate : Candidates)
	{
		if ((SelectedCandidate.Key->GetActorLocation() - CurrentLocation).Size() < MaxRelevanceDistance)
		{
			CheckRelativeDangerosity(MostDangerousCandidateActor, MostDangerousLocation, MostDangerousTimeToHit, MostDangerousInterceptDepth,
									 SelectedCandidate.Key, CurrentLocation, CurrentSize, SelectedCandidate.Value, CurrentVelocity, SpeedLimit);
		}
	}

	return MostDangerousCandidateActor != NULL;
}

bool PilotHelper::IsAnticollisionImminent(AFlareSpacecraft* Ship, float PreventionDuration, float SpeedLimit)
{
	// Output data
	AActor* MostDangerousCandidateActor;
	FVector MostDangerousLocation;
	float MostDangerousTimeToHit = PreventionDuration;
	float MostDangerousInterseptDepth;

	bool HaveCollision = FindMostDangerousCollision(MostDangerousCandidateActor, MostDangerousLocation, MostDangerousTimeToHit, MostDangerousInterseptDepth, Ship, AnticollisionConfig(), SpeedLimit);

	if (HaveCollision && Ship->IsLoadedAndReady() && !Ship->GetNavigationSystem()->IsAutoPilot() && !Ship->GetNavigationSystem()->IsDocked() && MostDangerousCandidateActor)
	{
		return true;
	}

	return false;
}

FVector PilotHelper::AnticollisionCorrection(AFlareSpacecraft* Ship, FVector InitialVelocity, float PreventionDuration, AnticollisionConfig Config, float SpeedLimit)
{
	SCOPE_CYCLE_COUNTER(STAT_PilotHelper_AnticollisionCorrection);

	//FLOGV("Anticollision for %s, PreventionDuration=%f", *Ship->GetImmatriculation().ToString(), PreventionDuration);

	// Output data
	AActor* MostDangerousCandidateActor = NULL;
	FVector MostDangerousLocation;
	float MostDangerousTimeToHit = PreventionDuration;
	float MostDangerousIntersectDepth;

	bool HaveCollision = FindMostDangerousCollision(MostDangerousCandidateActor, MostDangerousLocation, MostDangerousTimeToHit,MostDangerousIntersectDepth,
													Ship, Config, SpeedLimit);

	if(!HaveCollision)
	{
		return InitialVelocity;
	}

	// Avoid the most dangerous target
	if (MostDangerousCandidateActor)
	{
		SCOPE_CYCLE_COUNTER(STAT_PilotHelper_AnticollisionCorrection_Avoidance);

		FBox ShipBox = Ship->GetComponentsBoundingBox();
//		FBox ShipBox = Ship->GetMeshBox();
		FVector CurrentLocation = (ShipBox.Max + ShipBox.Min) / 2.0;

		if (MostDangerousTimeToHit > 0)
		{
			UPrimitiveComponent* StaticMeshComponent = Cast<UPrimitiveComponent>(MostDangerousCandidateActor->GetRootComponent());
			

			FVector HitDistancePoint = CurrentLocation + Ship->Airframe->GetPhysicsLinearVelocity() * MostDangerousTimeToHit;
			FVector FutureTargetLocation =  MostDangerousLocation + StaticMeshComponent->GetPhysicsLinearVelocity() * MostDangerousTimeToHit;
			FVector AvoidanceVector = HitDistancePoint - FutureTargetLocation;
			FVector AvoidanceAxis;

			if (AvoidanceVector.IsNearlyZero())
			{
				AvoidanceAxis = FMath::VRand();
			}
			else
			{
				AvoidanceAxis = AvoidanceVector.GetUnsafeNormal();
			}

			/*UKismetSystemLibrary::DrawDebugLine(Ship->GetWorld(), CurrentLocation, HitDistancePoint , FColor::Yellow, true);
			UKismetSystemLibrary::DrawDebugLine(Ship->GetWorld(), CurrentLocation, CurrentLocation + Ship->Airframe->GetPhysicsLinearVelocity(), FColor::White, true);
			UKismetSystemLibrary::DrawDebugLine(Ship->GetWorld(), CurrentLocation, CurrentLocation + AvoidanceAxis *1000 , FColor::Green, true);
			UKismetSystemLibrary::DrawDebugLine(Ship->GetWorld(), CurrentLocation, CurrentLocation + InitialVelocity *10 , FColor::Blue, true);
			FLOGV("MostDangerousTimeToHit %f", MostDangerousTimeToHit);*/

			// Below few second begin avoidance maneuver
			float Alpha = 1 - FMath::Max(0.0f, MostDangerousTimeToHit)/PreventionDuration;

			//FLOGV("InitialVelocity=%s", *InitialVelocity.ToString());
			//FLOGV("InitialVelocity.GetUnsafeNormal()=%s", *InitialVelocity.GetUnsafeNormal().ToString());

			if(Config.SpeedCorrectionOnly)
			{
				return InitialVelocity * (1.f - Alpha);
			}
			else
			{
				if(InitialVelocity.IsNearlyZero())
				{
					return AvoidanceAxis * Alpha  * Ship->GetNavigationSystem()->GetLinearMaxVelocity() * 100;
				}
				else
				{
					FVector Temp = InitialVelocity.GetUnsafeNormal() * (1.f - Alpha) + Alpha * AvoidanceAxis;
					Temp *= InitialVelocity.Size();
					//UKismetSystemLibrary::DrawDebugLine(Ship->GetWorld(), CurrentLocation, CurrentLocation + Temp * 10, FColor::Magenta, true);
					return Temp;
				}
			}


		}
		else
		{
			if(Config.SpeedCorrectionOnly)
			{
				return FVector::ZeroVector;
			}
			else
			{
				FVector Temp = (CurrentLocation - MostDangerousLocation).GetUnsafeNormal() * Ship->GetNavigationSystem()->GetLinearMaxVelocity();

				//UKismetSystemLibrary::DrawDebugLine(Ship->GetWorld(), CurrentLocation, CurrentLocation + Temp * 10, FColor::Red, true);
				return Temp;
			}
		}
	}

	return InitialVelocity;
}

bool PilotHelper::IsSectorExitImminent(AFlareSpacecraft* Ship, float PreventionDuration)
{
	float Distance = Ship->GetActorLocation().Size();
	float Limits = Ship->GetGame()->GetActiveSector()->GetSectorLimits();

	float MinDistance = Limits - Distance;
	bool ExitImminent = false;

	// Facing the center of the sector : no warning
	if (FVector::DotProduct(Ship->GetActorRotation().Vector(), -Ship->GetActorLocation()) > 0.5)
	{
		ExitImminent = false;
	}

	// Else, if close to limits : warning
	else if (MinDistance < 0.1 * Limits)
	{
		ExitImminent = true;
	}

	// Else if going to exit soon : warning
	else if (!Ship->GetLinearVelocity().IsNearlyZero())
	{
		// https://en.wikipedia.org/wiki/Line%E2%80%93sphere_intersection
		FVector ShipDirection = Ship->GetLinearVelocity().GetUnsafeNormal();
		FVector ShipLocation = Ship->GetActorLocation();
		float Dot = FVector::DotProduct(ShipDirection, ShipLocation);

		float DistanceBeforeExit = -Dot + FMath::Sqrt(Dot * Dot - Distance * Distance + Limits * Limits);
		float Velocity = Ship->GetLinearVelocity().Size() * 100;
		float DurationBeforeExit = DistanceBeforeExit / Velocity;

		if (DurationBeforeExit < PreventionDuration)
		{
			ExitImminent = true;
		}
	}

	return ExitImminent;
}

PilotHelper::PilotTarget PilotHelper::GetBestTarget(AFlareSpacecraft* Ship, struct TargetPreferences Preferences)
{
	SCOPE_CYCLE_COUNTER(STAT_PilotHelper_GetBestTarget);

	if (!Ship || !Ship->GetGame()->GetActiveSector())
	{
		return PilotHelper::PilotTarget();
	}

	PilotTarget BestTarget;
	float BestScore = 0;

	//FLOGV("GetBestTarget for %s", *Ship->GetImmatriculation().ToString());

	for (AFlareSpacecraft* ShipCandidate :Ship->GetGame()->GetActiveSector()->GetSpacecrafts())
	{
		if (!IsValid(ShipCandidate))
		{
			continue;
		}

		if (Preferences.IgnoreList.Contains(PilotTarget(ShipCandidate)))
		{
			continue;
		}

		if (!ShipCandidate->IsHostile(Ship->GetCompany()))
		{
			// Ignore not hostile ships
			continue;
		}

		if (!ShipCandidate->GetParent()->GetDamageSystem()->IsAlive())
		{
			// Ignore destroyed ships
			continue;
		}

		if (ShipCandidate->GetActorLocation().Size() > ShipCandidate->GetGame()->GetActiveSector()->GetSectorLimits())
		{
			// Ignore out limit ships
			continue;
		}

		float Score;
		float StateScore;
		float AttackTargetScore;
		float DistanceScore;
		float AlignementScore;

		StateScore = Preferences.TargetStateWeight;

		if (ShipCandidate->GetParent()->GetSize() == EFlarePartSize::L)
		{
			StateScore *= Preferences.IsLarge;
		}

		else if (ShipCandidate->GetParent()->GetSize() == EFlarePartSize::S)
		{
			StateScore *= Preferences.IsSmall;
		}

		if (ShipCandidate->GetParent()->IsStation())
		{
			if (Ship->GetCompany()->IsPlayerCompany() || (ShipCandidate->GetCompany()->IsPlayerCompany() && ShipCandidate->GetCompany()->GetRetaliation() > 0))
			{
				// All non player company, attack player station if there is retaliation
				StateScore *= Preferences.IsStation;
			}
			else
			{
				StateScore *= 0;
			}
		}
		else
		{
			StateScore *= Preferences.IsNotStation;
		}

		if (ShipCandidate->GetParent()->IsMilitary())
		{
			StateScore *= Preferences.IsMilitary;
		}
		else
		{
			StateScore *= Preferences.IsNotMilitary;
		}

		if (IsTargetDangerous(PilotTarget(ShipCandidate)))
		{
			StateScore *= Preferences.IsDangerous;
		}
		else
		{
			StateScore *= Preferences.IsNotDangerous;
		}

		if (ShipCandidate->GetParent()->GetDamageSystem()->IsStranded())
		{
			StateScore *= Preferences.IsStranded;
		}
		else
		{
			StateScore *= Preferences.IsNotStranded;
		}

		if (ShipCandidate->GetParent()->GetDamageSystem()->IsUncontrollable() && ShipCandidate->GetParent()->GetDamageSystem()->IsDisarmed())
		{
			if (ShipCandidate->IsMilitary())
			{
				if (ShipCandidate->GetSize() == EFlarePartSize::S)
				{
					StateScore *= Preferences.IsUncontrollableSmallMilitary;
				}
				else
				{
					StateScore *= Preferences.IsUncontrollableLargeMilitary;
				}
			}
			else
			{
				StateScore *= Preferences.IsUncontrollableCivil;
			}
		}
		else
		{
			StateScore *= Preferences.IsNotUncontrollable;
		}

		// Divise by 25 the stateScore per current incoming missile

		int32 IncomingBombs = ShipCandidate->GetIncomingActiveBombQuantity();
		if (IncomingBombs > 0)
		{
			StateScore /= (25 * IncomingBombs);
		}

		if(ShipCandidate->GetParent()->IsHarpooned()) {
			if(ShipCandidate->GetParent()->GetDamageSystem()->IsUncontrollable())
			{
				// Never target harponned uncontrollable ships
				continue;
			}
			StateScore *=  Preferences.IsHarpooned;
		}


		if(Preferences.LastTarget.Is(ShipCandidate)) {
			StateScore *=  Preferences.LastTargetWeight;
		}

		float Distance = (Preferences.BaseLocation - ShipCandidate->GetActorLocation()).Size();
		if (Distance >= Preferences.MaxDistance)
		{
			DistanceScore = 0.f;
		}
		else
		{
			DistanceScore = Preferences.DistanceWeight * (1.f - (Distance / Preferences.MaxDistance));
		}

		if (Preferences.AttackTarget && IsTargetDangerous(PilotTarget(ShipCandidate)) && ShipCandidate->GetPilot()->GetPilotTarget().Is(Preferences.AttackTarget))
		{
			AttackTargetScore = Preferences.AttackTargetWeight;
		}
		else
		{
			AttackTargetScore = 0.0f;
		}

		if(IsTargetDangerous(PilotTarget(ShipCandidate)) && ShipCandidate->GetPilot()->GetPilotTarget().Is(Ship))
		{
			StateScore *= Preferences.AttackMeWeight;
		}


		FVector Direction = (ShipCandidate->GetActorLocation() - Preferences.BaseLocation).GetUnsafeNormal();


		float Alignement = FVector::DotProduct(Preferences.PreferredDirection, Direction);

		if (Alignement > Preferences.MinAlignement)
		{
			AlignementScore = Preferences.AlignementWeight * ((Alignement - Preferences.MinAlignement) / (1 - Preferences.MinAlignement));
		}
		else
		{
			AlignementScore = 0;
		}

		Score = StateScore * (AttackTargetScore + DistanceScore + AlignementScore);


		/*FLOGV("  - %s: %f", *ShipCandidate->GetImmatriculation().ToString(), Score);
		FLOGV("        - StateScore=%f", StateScore);
		FLOGV("        - AttackTargetScore=%f", AttackTargetScore);
		FLOGV("        - DistanceScore=%f", DistanceScore);
		FLOGV("        - AlignementScore=%f", AlignementScore);*/

		if (Score > 0)
		{
			if (BestTarget.IsEmpty() || Score > BestScore)
			{
				BestTarget.SetSpacecraft(ShipCandidate);
				BestScore = Score;
			}
		}
	}

	for (AFlareBomb* BombCandidate :Ship->GetGame()->GetActiveSector()->GetBombs())
	{
		if (BombCandidate->IsSafeDestroying())
		{
			continue;
		}

		if (Preferences.IgnoreList.Contains(PilotTarget(BombCandidate)))
		{
			continue;
		}

		UPrimitiveComponent* RootComponent = Cast<UPrimitiveComponent>(BombCandidate->GetRootComponent());
		FVector DeltaVelocity = RootComponent->GetPhysicsLinearVelocity() - Ship->GetLinearVelocity() * 100;
		FVector DeltaLocation = BombCandidate->GetActorLocation() - Ship->GetActorLocation();

		bool Approching = true;

		if (DeltaVelocity.IsNearlyZero() || (FVector::DotProduct(DeltaLocation.GetUnsafeNormal(), DeltaVelocity) > 0))
		{
			Approching = false;
		}

		if (!BombCandidate->IsActive() && !Approching)
		{
			continue;
		}

		if (!BombCandidate->IsHostile(Ship->GetCompany()))
		{
			// Ignore not hostile bomb
			continue;
		}
		if (BombCandidate->GetActorLocation().Size() > Ship->GetGame()->GetActiveSector()->GetSectorLimits())
		{
			// Ignore out limit ships
			continue;
		}


		float Score;
		float StateScore;
		float AttackTargetScore;
		float DistanceScore;
		float AlignementScore;

		StateScore = Preferences.TargetStateWeight * Preferences.IsBomb;

		if(Preferences.LastTarget.Is(BombCandidate)) {
			StateScore *=  Preferences.LastTargetWeight;
		}

		float Distance = (Preferences.BaseLocation - BombCandidate->GetActorLocation()).Size();

		if(Distance >= Preferences.MaxBombDistance)
		{
			continue;
		}

		if (Distance >= Preferences.MaxDistance)
		{
			DistanceScore = 0.f;
		}
		else
		{
			DistanceScore = Preferences.DistanceWeight * (1.f - (Distance / Preferences.MaxDistance));
		}

		if (Preferences.AttackTarget && BombCandidate->GetTargetSpacecraft() == Preferences.AttackTarget)
		{
			AttackTargetScore = Preferences.AttackTargetWeight;
		}
		else
		{
			AttackTargetScore = 0.0f;
		}


		if(BombCandidate->GetTargetSpacecraft() == Ship)
		{
			StateScore *= Preferences.AttackMeWeight;
		}


		FVector Direction = (BombCandidate->GetActorLocation() - Preferences.BaseLocation).GetUnsafeNormal();


		float Alignement = FVector::DotProduct(Preferences.PreferredDirection, Direction);

		if (Alignement > Preferences.MinAlignement)
		{
			AlignementScore = Preferences.AlignementWeight * ((Alignement - Preferences.MinAlignement) / (1 - Preferences.MinAlignement));
		}
		else
		{
			AlignementScore = 0;
		}

		Score = StateScore * (AttackTargetScore + DistanceScore + AlignementScore);


		/*FLOGV("  - %s: %f", *ShipCandidate->GetImmatriculation().ToString(), Score);
		FLOGV("        - StateScore=%f", StateScore);
		FLOGV("        - AttackTargetScore=%f", AttackTargetScore);
		FLOGV("        - DistanceScore=%f", DistanceScore);
		FLOGV("        - AlignementScore=%f", AlignementScore);*/

		if (Score > 0)
		{
			if (BestTarget.IsEmpty() || Score > BestScore)
			{
				BestTarget.SetBomb(BombCandidate);
				BestScore = Score;
			}
		}

	}

	for (AFlareMeteorite* MeteoriteCandidate :Ship->GetGame()->GetActiveSector()->GetMeteorites())
	{
		if (Preferences.IgnoreList.Contains(PilotTarget(MeteoriteCandidate)))
		{
			continue;
		}

		if (MeteoriteCandidate->GetActorLocation().Size() > Ship->GetGame()->GetActiveSector()->GetSectorLimits())
		{
			// Ignore out limit ships
			continue;
		}

		if (MeteoriteCandidate->IsBroken())
		{
			continue;
		}

		if (MeteoriteCandidate->HasMissed())
		{
			continue;
		}

		float Score;
		float StateScore;
		float AttackTargetScore;
		float DistanceScore;
		float AlignementScore;

		StateScore = Preferences.TargetStateWeight * Preferences.IsMeteorite;


		if(Preferences.LastTarget.Is(MeteoriteCandidate)) {
			StateScore *=  Preferences.LastTargetWeight;
		}

		float Distance = (Preferences.BaseLocation - MeteoriteCandidate->GetActorLocation()).Size();
		if (Distance >= Preferences.MaxDistance)
		{
			DistanceScore = 0.f;
		}
		else
		{
			DistanceScore = Preferences.DistanceWeight * (1.f - (Distance / Preferences.MaxDistance));
		}


		AttackTargetScore = 0.0f;

		FVector Direction = (MeteoriteCandidate->GetActorLocation() - Preferences.BaseLocation).GetUnsafeNormal();


		float Alignement = FVector::DotProduct(Preferences.PreferredDirection, Direction);

		if (Alignement > Preferences.MinAlignement)
		{
			AlignementScore = Preferences.AlignementWeight * ((Alignement - Preferences.MinAlignement) / (1 - Preferences.MinAlignement));
		}
		else
		{
			AlignementScore = 0;
		}

		Score = StateScore * (AttackTargetScore + DistanceScore + AlignementScore);

		if (Score > 0)
		{
			if (BestTarget.IsEmpty() || Score > BestScore)
			{
				BestTarget.SetMeteorite(MeteoriteCandidate);
				BestScore = Score;
			}
		}

	}

	/*if(BestTarget)
	{
		FLOGV(" -> BestTarget %s with %f", *BestTarget->GetImmatriculation().ToString(), BestScore);
	}
	else
	{
		FLOG(" -> No target");
	}*/

	return BestTarget;
}


UFlareSpacecraftComponent* PilotHelper::GetBestTargetComponent(AFlareSpacecraft* TargetSpacecraft)
{
	SCOPE_CYCLE_COUNTER(STAT_PilotHelper_GetBestTargetComponent);

	if (!TargetSpacecraft)
	{
		return NULL;
	}
	// Is armed, target the gun
	// Else if not stranger target the orbital
	// else target the rsc

	float WeaponWeight = 1;
	float PodWeight = 1;
	float RCSWeight = 1;
	float InternalWeight = 1;

	if (!TargetSpacecraft->GetParent()->GetDamageSystem()->IsDisarmed())
	{
		WeaponWeight = 10;
		PodWeight = 4;
	}
	else if (!TargetSpacecraft->GetParent()->GetDamageSystem()->IsStranded())
	{
		PodWeight = 5;
	}

	TArray<UFlareSpacecraftComponent*> ComponentSelection;

	TArray<UActorComponent*> Components = TargetSpacecraft->GetComponentsByClass(UFlareSpacecraftComponent::StaticClass());
	for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
	{
		UFlareSpacecraftComponent* Component = Cast<UFlareSpacecraftComponent>(Components[ComponentIndex]);

		if (Component->GetDescription() && !Component->IsBroken() )
		{
			UFlareRCS* RCS = Cast<UFlareRCS>(Component);
			if (RCS)
			{
				ComponentSelection.Reserve(ComponentSelection.Num() + RCSWeight);
				for (int32 i = 0; i < RCSWeight; i++)
				{
					ComponentSelection.Add(Component);
				}
			}

			UFlareOrbitalEngine* OrbitalEngine = Cast<UFlareOrbitalEngine>(Component);
			if (OrbitalEngine)
			{
				ComponentSelection.Reserve(ComponentSelection.Num() + PodWeight);
				for (int32 i = 0; i < PodWeight; i++)
				{
					ComponentSelection.Add(Component);
				}
			}

			UFlareWeapon* Weapon = Cast<UFlareWeapon>(Component);
			if (Weapon)
			{
				ComponentSelection.Reserve(ComponentSelection.Num() + WeaponWeight);
				for (int32 i = 0; i < WeaponWeight; i++)
				{
					ComponentSelection.Add(Component);
				}
			}

			if (Component->GetDescription()->Type == EFlarePartType::InternalComponent)
			{
				ComponentSelection.Reserve(ComponentSelection.Num() + InternalWeight);
				for (int32 i = 0; i < InternalWeight; i++)
				{
					ComponentSelection.Add(Component);
				}
			}
		}
	}

	if (ComponentSelection.Num() == 0)
	{
		return NULL;
	}

	int32 ComponentIndex = FMath::RandRange(0, ComponentSelection.Num() - 1);
	return ComponentSelection[ComponentIndex];
}

bool PilotHelper::CheckRelativeDangerosity(AActor*& MostDangerousCandidateActor, FVector& MostDangerousLocation, float& MostDangerousTimeToHit, float& MostDangerousInterseptDepth, AActor* CandidateActor, FVector CurrentLocation, float CurrentSize, FVector TargetVelocity, FVector CurrentVelocity, float SpeedLimit)
{
	SCOPE_CYCLE_COUNTER(STAT_PilotHelper_CheckRelativeDangerosity);
	//FLOGV("PilotHelper::CheckRelativeDangerosity for %s, ship size %f", *CandidateActor->GetName(), CurrentSize);

	FVector CandidateLocation = CandidateActor->GetActorLocation();
	FVector DeltaVelocity = TargetVelocity - CurrentVelocity;
	FVector DeltaLocation = CandidateLocation - CurrentLocation;

	// Eliminate obvious not-dangerous candidates based on velocity

	//FLOGV("TargetVelocity %s", *TargetVelocity.ToString());
	//FLOGV("CurrentVelocity %s", *CurrentVelocity.ToString());
	//FLOGV("Vector::DotProduct(DeltaLocation.GetUnsafeNormal(), DeltaVelocity) %f", FVector::DotProduct(DeltaLocation.GetUnsafeNormal(), DeltaVelocity));

	if (DeltaVelocity.IsNearlyZero() || (FVector::DotProduct(DeltaLocation.GetUnsafeNormal(), DeltaVelocity) > -SpeedLimit))
	{
		return false;
	}
	
	// Get the object size & location
	float CandidateSize;
	if (CandidateActor->IsA(AFlareCollider::StaticClass()) || CandidateActor->IsA(AFlareAsteroid::StaticClass()))
	{
		CandidateSize = Cast<UStaticMeshComponent>(CandidateActor->GetRootComponent())->Bounds.SphereRadius;
	}
/*
	else if (CandidateActor->IsA(AFlareSpacecraft::StaticClass()))
	{
		AFlareSpacecraft* CandidateSpacecraft = Cast<AFlareSpacecraft>(CandidateActor);
		FBox CandidateBox = CandidateSpacecraft->GetMeshBox();
		CandidateSize = CandidateSpacecraft->GetMeshScale();
		CandidateLocation = (CandidateBox.Max + CandidateBox.Min) / 2.0;
	}
*/
	else
	{
		FBox CandidateBox = CandidateActor->GetComponentsBoundingBox();
		CandidateSize = FMath::Max(CandidateBox.GetExtent().Size(), 1.0f);
		CandidateLocation = (CandidateBox.Max + CandidateBox.Min) / 2.0;
	}

	FVector DeltaVelocityDirection = DeltaVelocity.GetUnsafeNormal();

	// Minimum distance highter than object size sum : not dangerous
	float TargetPointToMinPointDistance = FVector::DotProduct(DeltaLocation, -DeltaVelocityDirection);
	FVector MinPointLocation = CandidateLocation + DeltaVelocityDirection * TargetPointToMinPointDistance;
	float MinPointToCurrentPointDistance = (CurrentLocation - MinPointLocation).Size();

	float SizeSum = CurrentSize + CandidateSize;
	if (SizeSum < MinPointToCurrentPointDistance)
	{
		return false;
	}

	// Already intersecting ?
	if (DeltaLocation.Size() < SizeSum)
	{
		float IntersectDeep = SizeSum - DeltaLocation.Size();
		if (!MostDangerousCandidateActor || MostDangerousTimeToHit > 0 || MostDangerousInterseptDepth < IntersectDeep)
		{
			MostDangerousCandidateActor = CandidateActor;
			MostDangerousTimeToHit = 0;
			MostDangerousInterseptDepth = IntersectDeep;
			MostDangerousLocation = CandidateLocation;
			return true;
		}
	}

	if (MostDangerousTimeToHit == 0)
	{
		// Future hit is never more dangerous than current intersect
		return false;
	}

	float MinPointToHitPointDistance = FMath::Sqrt(FMath::Square(SizeSum) - FMath::Square(MinPointToCurrentPointDistance));
	float DistanceToHit = TargetPointToMinPointDistance - MinPointToHitPointDistance;
	float TimeToHit = DistanceToHit / DeltaVelocity.Size();


	// Time to hit pointis high : not dangerous
	if (TimeToHit > MostDangerousTimeToHit)
	{
		return false;
	}
	
	// Keep only most imminent hit
	if (!MostDangerousCandidateActor || MostDangerousTimeToHit > TimeToHit)
	{
		MostDangerousCandidateActor = CandidateActor;
		MostDangerousTimeToHit = TimeToHit;
		MostDangerousInterseptDepth = 0;
		MostDangerousLocation = CandidateLocation;

		/*FLOGV("CandidateActor %s", *CandidateActor->GetName());
		FLOGV("CandidateActor %s", *CandidateActor->GetName());



		FLOGV("DistanceToMinDistancePoint %f", DistanceToMinDistancePoint)
		FLOGV("DeltaLocation %s", *DeltaLocation.ToString())
		FLOGV("MinDistance %f", MinDistance)
		FLOGV("DeltaVelocity %s", *DeltaVelocity.ToString())
		FLOGV("TargetVelocity %s", *TargetVelocity.ToString())
		FLOGV("CurrentVelocity %s", *CurrentVelocity.ToString())

		FLOGV("TimeToMinDistance %f", TimeToMinDistance)
		FLOGV("InterCollisionTravelTime %f", InterCollisionTravelTime)*/



		//UKismetSystemLibrary::DrawDebugLine(CandidateActor->GetWorld(), CandidateLocation, Box.Max , FColor::Yellow, true);
		//UKismetSystemLibrary::DrawDebugLine(CandidateActor->GetWorld(), CandidateLocation, Box.Min , FColor::Green, true);
		//UKismetSystemLibrary::DrawDebugSphere(CandidateActor->GetWorld(), CandidateLocation, 10, 12, FColor::White, true);

		//DrawDebugBox(CandidateActor->GetWorld(), BoxLocation, BoxSize, FQuat(FRotator::ZeroRotator), FColor::Blue, 1.f);
		//DrawDebugBox(CandidateActor->GetWorld(), Box2Location, Box2Size, FQuat(FRotator::ZeroRotator), FColor::Cyan, 1.f);

		//UKismetSystemLibrary::DrawDebugSphere(CandidateActor->GetWorld(), CandidateLocation, CandidateSize, 12, FColor::Red, true);
		//UKismetSystemLibrary::DrawDebugSphere(CandidateActor->GetWorld(), CurrentLocation, CandidateActor->GetMeshScale(), 12, FColor::Green, true);
		//UKismetSystemLibrary::DrawDebugSphere(CandidateActor->GetWorld(), Box2Location, CandidateSize * 2.f, 12, FColor::Magenta, true);
		return true;
	}
	return false;
}

bool PilotHelper::IsTargetDangerous(PilotHelper::PilotTarget const& Target)
{
	if(Target.SpacecraftTarget)
	{
		return Target.SpacecraftTarget->GetParent()->IsMilitaryArmed() && !Target.SpacecraftTarget->GetParent()->GetDamageSystem()->IsDisarmed();
	}
	else if(Target.BombTarget)
	{
		return true;
	}

	return false;
}

bool PilotHelper::PilotTarget::IsEmpty() const
{
	return SpacecraftTarget == nullptr && MeteoriteTarget == nullptr && BombTarget == nullptr;
}

bool PilotHelper::PilotTarget::IsValid()
{
	if (SpacecraftTarget)
	{
		if (SpacecraftTarget->IsActorBeingDestroyed() || SpacecraftTarget->IsSafeEither() || SpacecraftTarget->CheckIsExploding())
		{
			SpacecraftTarget = nullptr;
			return false;
		}
	}
	return SpacecraftTarget != nullptr || MeteoriteTarget != nullptr || BombTarget != nullptr;
}

bool PilotHelper::PilotTarget::Is(AFlareSpacecraft* Spacecraft) const
{
	return SpacecraftTarget == Spacecraft;
}

bool PilotHelper::PilotTarget::Is(AFlareMeteorite* Meteorite) const
{
	return MeteoriteTarget == Meteorite;
}

bool PilotHelper::PilotTarget::Is(AFlareBomb* Bomb) const
{
	return BombTarget == Bomb;
}

void PilotHelper::PilotTarget::Clear()
{
	SpacecraftTarget = nullptr;
	MeteoriteTarget = nullptr;
	BombTarget = nullptr;
}

void PilotHelper::PilotTarget::SetSpacecraft(AFlareSpacecraft* Spacecraft)
{
	Clear();
	SpacecraftTarget = Spacecraft;
}

void PilotHelper::PilotTarget::SetMeteorite(AFlareMeteorite* Meteorite)
{
	Clear();
	MeteoriteTarget = Meteorite;
}

void PilotHelper::PilotTarget::SetBomb(AFlareBomb* Bomb)
{
	Clear();
	BombTarget = Bomb;
}

FVector PilotHelper::PilotTarget::GetActorLocation() const
{
	if(SpacecraftTarget&&SpacecraftTarget!=nullptr)
	{
		if (!SpacecraftTarget->IsActorBeingDestroyed() && !SpacecraftTarget->IsPendingKill())
		{
			return SpacecraftTarget->GetActorLocation();
		}
	}

	if(BombTarget&&BombTarget!=nullptr)
	{
		if (!BombTarget->IsSafeDestroying() && !BombTarget->IsActorBeingDestroyed() && !BombTarget->IsPendingKill())
		{
			return BombTarget->GetActorLocation();
		}
	}

	if(MeteoriteTarget)
	{
		if (!MeteoriteTarget->IsActorBeingDestroyed() && !MeteoriteTarget->IsPendingKill())
		{
			return MeteoriteTarget->GetActorLocation();
		}
	}

	return FVector::ZeroVector;
}

FVector PilotHelper::PilotTarget::GetLinearVelocity() const
{
	if(SpacecraftTarget && SpacecraftTarget != nullptr)
	{
		if (!SpacecraftTarget->IsSafeDestroying() && !SpacecraftTarget->IsPendingKill())
		{
			return SpacecraftTarget->Airframe->GetPhysicsLinearVelocity();
		}
	}

	if (BombTarget && BombTarget != nullptr)
	{
		if (!BombTarget->IsSafeDestroying() && !BombTarget->IsActorBeingDestroyed() && !BombTarget->IsPendingKill())
		{
			return Cast<UPrimitiveComponent>(BombTarget->GetRootComponent())->GetPhysicsLinearVelocity();
		}
	}

	if(MeteoriteTarget && MeteoriteTarget != nullptr)
	{
		if (!MeteoriteTarget->IsActorBeingDestroyed() && !MeteoriteTarget->IsPendingKill())
		{
			return Cast<UPrimitiveComponent>(MeteoriteTarget->GetRootComponent())->GetPhysicsLinearVelocity();
		}
	}
	return FVector(0, 0, 0);
}


float PilotHelper::PilotTarget::GetMeshScale()
{
	if(SpacecraftTarget)
	{
		if (!SpacecraftTarget->IsActorBeingDestroyed() && !SpacecraftTarget->IsPendingKill())
		{
			return SpacecraftTarget->GetMeshScale();
		}
	}

	if(BombTarget)
	{
		if (!BombTarget->IsSafeDestroying() && !BombTarget->IsActorBeingDestroyed() && !BombTarget->IsPendingKill())
		{
			FBox Box = BombTarget->GetComponentsBoundingBox();
			return FMath::Max(Box.GetExtent().Size(), 1.0f);
		}
	}

	if(MeteoriteTarget)
	{
		if (!MeteoriteTarget->IsActorBeingDestroyed() && !MeteoriteTarget->IsPendingKill())
		{
			FBox Box = MeteoriteTarget->GetComponentsBoundingBox();
			return FMath::Max(Box.GetExtent().Size(), 1.0f);
		}
	}

	return 0.f;
}

float PilotHelper::PilotTarget::GetAimPosition(AFlareSpacecraft* TargettingShip, float BulletSpeed, float PredictionDelay, FVector* ResultPosition) const
{
	return SpacecraftHelper::GetIntersectionPosition(GetActorLocation(),
											  GetLinearVelocity(),
											  TargettingShip->GetCamera()->GetComponentLocation(),
											  TargettingShip->GetLinearVelocity() * 100,
											  BulletSpeed * 100,
											  PredictionDelay,
											  ResultPosition);
}

AActor* PilotHelper::PilotTarget::GetActor()
{
	if(SpacecraftTarget)
	{
		return SpacecraftTarget;
	}

	if(BombTarget)
	{
		return BombTarget;
	}

	if(MeteoriteTarget)
	{
		return MeteoriteTarget;
	}
	return nullptr;
}
