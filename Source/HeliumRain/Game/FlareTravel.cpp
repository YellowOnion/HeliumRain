

#include "FlareTravel.h"
#include "../Flare.h"

#include "../Data/FlareResourceCatalog.h"

#include "../Economy/FlareCargoBay.h"

#include "FlareWorld.h"
#include "FlareGame.h"
#include "FlareGameTools.h"
#include "FlareSectorHelper.h"

#include "../Player/FlarePlayerController.h"

#include "../UI/Components/FlareNotification.h"

//static const double TRAVEL_DURATION_PER_PHASE_KM = 0.4;
//static const double TRAVEL_DURATION_PER_ALTITUDE_KM = 1.5;


static const double TRAVEL_DURATION_PER_PHASE_KM = 0.52;
static const double TRAVEL_DURATION_PER_ALTITUDE_KM = 2;
//TRAVEL_DURATION_PER_ALTITUDE_KM * 0.26 = TRAVEL_DURATION_PER_PHASE_KM

#define LOCTEXT_NAMESPACE "FlareTravelInfos"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareTravel::UFlareTravel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UFlareTravel::InitTravelSector(FFlareSectorSave& NewSectorData)
{
	// Init new travel sector
	NewSectorData.GivenName = FText();
	NewSectorData.Identifier = TEXT("Travel");
	NewSectorData.LocalTime = 0;
	NewSectorData.IsTravelSector = true;

	// Init population
	NewSectorData.PeopleData.Population = 0;
	NewSectorData.PeopleData.BirthPoint = 0;
	NewSectorData.PeopleData.DeathPoint = 0;
	NewSectorData.PeopleData.FoodStock = 0;
	NewSectorData.PeopleData.FuelStock = 0;
	NewSectorData.PeopleData.ToolStock = 0;
	NewSectorData.PeopleData.TechStock = 0;
	NewSectorData.PeopleData.FoodConsumption = 0;
	NewSectorData.PeopleData.FuelConsumption = 0;
	NewSectorData.PeopleData.ToolConsumption = 0;
	NewSectorData.PeopleData.TechConsumption = 0;
	NewSectorData.PeopleData.HappinessPoint = 0;
	NewSectorData.PeopleData.HungerPoint = 0;
	NewSectorData.PeopleData.Money = 0;
	NewSectorData.PeopleData.Dept = 0;
}

void UFlareTravel::Load(const FFlareTravelSave& Data, UFlareFleet* NewFleet)
{
	Game = Cast<UFlareWorld>(GetOuter())->GetGame();
	TravelData = Data;
	TravelShips.Empty();

	if (!NewFleet)
	{
		Fleet = Game->GetGameWorld()->FindFleet(TravelData.FleetIdentifier);
	}
	else
	{
		Fleet = NewFleet;
	}

	DestinationSector = Game->GetGameWorld()->FindSector(TravelData.DestinationSectorIdentifier);
	OldDestinationSector = DestinationSector;
	OriginSector = Game->GetGameWorld()->FindSector(TravelData.OriginSectorIdentifier);

	TravelShips.Reserve(Fleet->GetShips().Num());
	for (int ShipIndex = 0; ShipIndex < Fleet->GetShips().Num(); ShipIndex++)
	{
		TravelShips.Add(Fleet->GetShips()[ShipIndex]);
	}

	Fleet->SetCurrentTravel(this);
	GenerateTravelDuration();


	FFlareSectorOrbitParameters OrbitParameters;
	OrbitParameters = *OriginSector->GetOrbitParameters();


	SectorDescription.Name = LOCTEXT("TravelSectorName", "Traveling ...");
	SectorDescription.Description = LOCTEXT("TravelSectorDescription", "Travel sector");
	SectorDescription.Identifier=Fleet->GetIdentifier();
	SectorDescription.Phase = 0;
	SectorDescription.IsIcy = false;
	SectorDescription.IsGeostationary = false;
	SectorDescription.IsSolarPoor = false;
	SectorDescription.LevelName = NAME_None;
	SectorDescription.DebrisFieldInfo.DebrisCatalog = NULL;
	SectorDescription.LevelTrack = FMath::RandBool() ? EFlareMusicTrack::Travel1 : EFlareMusicTrack::Travel2;
	SectorDescription.DebrisFieldInfo.DebrisFieldDensity = 0;
	SectorDescription.DebrisFieldInfo.MaxDebrisSize = 0;
	SectorDescription.DebrisFieldInfo.MinDebrisSize = 0;

	TravelSector = NewObject<UFlareSimulatedSector>(this, UFlareSimulatedSector::StaticClass());
	TravelSector->Load(&SectorDescription, Data.SectorData, OrbitParameters);

	UpdateTravelParameters();

	Fleet->SetCurrentSector(TravelSector);
	TravelSector->AddFleet(Fleet);

	NeedNotification = true;
}


FFlareTravelSave* UFlareTravel::Save()
{
	return &TravelData;
}


/*----------------------------------------------------
	Gameplay
----------------------------------------------------*/

void UFlareTravel::Simulate()
{
	int64 RemainingTime = GetRemainingTravelDuration();

	UpdateTravelParameters();

	// Incoming player notification. This doesn't work for incoming enemies, as they don't travel in fleets.
	if (RemainingTime == 1)
	{
		UFlareCompany* PlayerCompany = Fleet->GetGame()->GetPC()->GetCompany();

		if (Fleet->GetFleetCompany() == PlayerCompany
			&& DestinationSector->GetSectorFriendlyness(PlayerCompany) >= EFlareSectorFriendlyness::Contested)
		{
			FFlareMenuParameterData Data;
			Data.Sector = DestinationSector;

			Game->GetPC()->Notify(LOCTEXT("TravelAttackingSoon", "Destination is defended"),
				FText::Format(LOCTEXT("TravelAttackingSoonFormat", "Your fleet is arriving at {0}, but this sector is defended. Prepare for battle."),
					DestinationSector->GetSectorName()),
				FName("travel-raid-soon"),
				EFlareNotification::NT_Military,
				false,
				EFlareMenu::MENU_Sector,
				Data);
		}
	}

	// Trael is done
	else if (RemainingTime <= 0)
	{
		EndTravel();
	}
}

void UFlareTravel::UpdateTravelParameters()
{
	if(OriginSector == DestinationSector)
	{
		TravelSector->SetSectorOrbitParameters(*OriginSector->GetOrbitParameters());
		return;
	}

	float TravelRatio = GetElapsedTime() /  float(TravelDuration);

	FFlareSectorOrbitParameters OrbitParameters;
	OrbitParameters.Phase = FMath::Lerp(OriginSector->GetOrbitParameters()->Phase, DestinationSector->GetOrbitParameters()->Phase, TravelRatio);

	if (OriginSector->GetOrbitParameters()->CelestialBodyIdentifier == DestinationSector->GetOrbitParameters()->CelestialBodyIdentifier)
	{
		OrbitParameters.CelestialBodyIdentifier = OriginSector->GetOrbitParameters()->CelestialBodyIdentifier;

		OrbitParameters.Altitude = FMath::Lerp(OriginSector->GetOrbitParameters()->Altitude, DestinationSector->GetOrbitParameters()->Altitude, TravelRatio);
	}
	else
	{
		FFlareCelestialBody* OriginBody = GetGame()->GetGameWorld()->GetPlanerarium()->FindCelestialBody(OriginSector->GetOrbitParameters()->CelestialBodyIdentifier);
		FFlareCelestialBody* DestinationBody = GetGame()->GetGameWorld()->GetPlanerarium()->FindCelestialBody(DestinationSector->GetOrbitParameters()->CelestialBodyIdentifier);

		double OriginBaseAltitude;
		double OriginLocalAltitude;
		double DestinationBaseAltitude;
		double DestinationLocalAltitude;

		if(OriginBody->Sattelites.Contains(DestinationBody))
		{
			// Nema to moon

			OriginBaseAltitude = 0;
			OriginLocalAltitude = OriginSector->GetOrbitParameters()->Altitude;

			DestinationBaseAltitude = DestinationBody->OrbitDistance;
			DestinationLocalAltitude = DestinationSector->GetOrbitParameters()->Altitude;

		}
		else if (DestinationBody->Sattelites.Contains(OriginBody))
		{
			// Moon to Nema
			OriginBaseAltitude = OriginBody->OrbitDistance;
			OriginLocalAltitude = OriginSector->GetOrbitParameters()->Altitude;

			DestinationBaseAltitude = 0;
			DestinationLocalAltitude = DestinationSector->GetOrbitParameters()->Altitude;
		}
		else
		{
			// Moon to moon
			OriginBaseAltitude = OriginBody->OrbitDistance;
			OriginLocalAltitude = OriginSector->GetOrbitParameters()->Altitude;

			DestinationBaseAltitude = DestinationBody->OrbitDistance;
			DestinationLocalAltitude = DestinationSector->GetOrbitParameters()->Altitude;
		}


		double Sign = FMath::Sign(DestinationBaseAltitude - OriginBaseAltitude);

		double OriginStartAltitude = OriginBaseAltitude + Sign * OriginLocalAltitude;
		double DestinationStartAltitude = DestinationBaseAltitude - Sign * OriginLocalAltitude;

		double LimitAltitude = (OriginStartAltitude + DestinationStartAltitude)/2;


		if(TravelRatio < 0.5)
		{
			OrbitParameters.CelestialBodyIdentifier = OriginSector->GetOrbitParameters()->CelestialBodyIdentifier;

			double CurrentAltitude = FMath::Lerp(OriginStartAltitude, LimitAltitude, TravelRatio*2);
			OrbitParameters.Altitude = FMath::Abs(CurrentAltitude - OriginBaseAltitude);
		}
		else
		{
			OrbitParameters.CelestialBodyIdentifier = DestinationSector->GetOrbitParameters()->CelestialBodyIdentifier;

			double CurrentAltitude = FMath::Lerp(OriginStartAltitude, LimitAltitude, TravelRatio*2);
			OrbitParameters.Altitude = FMath::Abs(CurrentAltitude - DestinationBaseAltitude);
		}
	}

	TravelSector->SetSectorOrbitParameters(OrbitParameters);
}


void UFlareTravel::EndTravel()
{
	// Each company has a chance of discovering the source sector
	if (GetSourceSector() && DestinationSector != GetSourceSector())
	{
		UFlareSimulatedSector* Source = GetSourceSector();
		if (!Source->GetDescription()->IsHiddenFromMovement)
		{
			float DiscoveryChance = 0.005;
			TArray<UFlareCompany*> SectorCompanies;

			// List companies
			for (auto Spacecraft : DestinationSector->GetSectorSpacecrafts())
			{
				SectorCompanies.AddUnique(Spacecraft->GetCompany());
			}

			for (auto Company : SectorCompanies)
			{
				if (!Company->IsKnownSector(Source) && Company != Fleet->GetFleetCompany())
				{
					if (FMath::FRand() < DiscoveryChance)
					{
						if (Company == Game->GetPC()->GetCompany())
						{
							FLOGV("UFlareTravel::EndTravel : player discovered '%s'", *Source->GetSectorName().ToString());
							Game->GetPC()->DiscoverSector(Source, false, true);
						}
						else
						{
							FLOGV("UFlareTravel::EndTravel : '%s' discovered '%s'", *Company->GetCompanyName().ToString(), *Source->GetSectorName().ToString());
							Company->DiscoverSector(Source);
						}
					}
				}
			}
		}
	}

	// Make fleet arrive
	Fleet->SetCurrentSector(DestinationSector);	
	DestinationSector->AddFleet(Fleet);

	// People migration
	OriginSector->GetPeople()->Migrate(DestinationSector, Fleet->GetShipCount());
	
	// Price migration
	for(int32 ResourceIndex = 0; ResourceIndex < Game->GetResourceCatalog()->Resources.Num(); ResourceIndex++)
	{
		FFlareResourceDescription* Resource = &Game->GetResourceCatalog()->Resources[ResourceIndex]->Data;

		float ContaminationFactor = 0.00f;
		for (int ShipIndex = 0; ShipIndex < Fleet->GetShips().Num(); ShipIndex++)
		{
			UFlareSimulatedSpacecraft* Ship = Fleet->GetShips()[ShipIndex];
			if (Ship->GetActiveCargoBay()->GetResourceQuantity(Resource, Ship->GetCompany()) > 0)
			{
				ContaminationFactor += Ship->GetActiveCargoBay()->GetResourceQuantity(Resource, Ship->GetCompany()) / 1000.f;
				//TODO scale bay world stock/flow
			}
		}

		ContaminationFactor = FMath::Min(ContaminationFactor, 1.f);

		float OriginPrice = OriginSector->GetPreciseResourcePrice(Resource);
		float DestinationPrice = DestinationSector->GetPreciseResourcePrice(Resource);
		float Mean = (OriginPrice + DestinationPrice) / 2.f;
		
		float NewOriginPrice = (OriginPrice * (1 - ContaminationFactor)) + (ContaminationFactor * Mean);
		float NewDestinationPrice = (DestinationPrice * (1 - ContaminationFactor)) + (ContaminationFactor * Mean);

		//FLOGV("Travel start from %s. %s price ajusted from %f to %f (Mean: %f)", *OriginSector->GetSectorName().ToString(), *Resource->Name.ToString(), OriginPrice/100., NewOriginPrice/100., Mean/100.);
		//FLOGV("Travel end from %s. %s price ajusted from %f to %f (Mean: %f)", *DestinationSector->GetSectorName().ToString(), *Resource->Name.ToString(), DestinationPrice/100., NewDestinationPrice/100., Mean/100.);

		OriginSector->SetPreciseResourcePrice(Resource, NewOriginPrice);
		DestinationSector->SetPreciseResourcePrice(Resource, NewDestinationPrice);
	}
	
	// Notify travel ended
	if (Fleet->GetFleetCompany() == Game->GetPC()->GetCompany()
		&& (Fleet->GetCurrentTradeRoute() == NULL || Fleet->GetCurrentTradeRoute()->IsPaused())
		&& !Fleet->IsAutoTrading())
	{
		FFlareMenuParameterData Data;
		Data.Sector = DestinationSector;
		Game->GetPC()->Notify(LOCTEXT("TravelEnded", "Travel ended"),
			FText::Format(LOCTEXT("TravelEndedFormat", "{0} arrived at {1}"),
				Fleet->GetFleetName(),
				DestinationSector->GetSectorName()),
			FName("travel-end"),
			EFlareNotification::NT_Info,
			false,
			EFlareMenu::MENU_Sector,
			Data);

		Game->GetQuestManager()->OnEvent(FFlareBundle().PutTag("travel-end").PutName("sector", DestinationSector->GetIdentifier()).PutName("fleet", Fleet->GetIdentifier()));
	}

	if (Fleet == Game->GetPC()->GetPlayerFleet())
	{

		bool HasLMilitaryShip = false;
		for (UFlareSimulatedSpacecraft* Ship :Fleet->GetShips())
		{
			if (Ship->IsMilitary() && Ship->GetSize() == EFlarePartSize::L)
			{
				HasLMilitaryShip = true;
				break;
			}
		}

		if(HasLMilitaryShip)
		{
			CompanyValue Value = Game->GetPC()->GetCompany()->GetCompanyValue(DestinationSector, false);
			int32 CurrentHostilePoints = SectorHelper::GetHostileArmyCombatPoints(DestinationSector, Game->GetPC()->GetCompany(), true);


			if(CurrentHostilePoints > Value.ArmyCurrentCombatPoints * 2)
			{
				Game->GetPC()->SetAchievementProgression("ACHIEVEMENT_TRAP", 1);
			}
		}
	}

	// Update game
	Game->GetGameWorld()->DeleteTravel(this);
	if (Game->GetQuestManager())
	{
		Game->GetQuestManager()->OnTravelEnded(Fleet);
	}
}

int64 UFlareTravel::GetElapsedTime()
{
	return Game->GetGameWorld()->GetDate() - TravelData.DepartureDate;
}

int64 UFlareTravel::GetRemainingTravelDuration()
{
	return TravelDuration - GetElapsedTime();
}

void UFlareTravel::ChangeDestination(UFlareSimulatedSector* NewDestinationSector)
{
	if (!CanChangeDestination())
	{
		return;
	}

	if(NewDestinationSector->IsTravelSector())
	{
		return;
	}

	OldDestinationSector = DestinationSector;
	DestinationSector = NewDestinationSector;

	TravelData.DestinationSectorIdentifier = DestinationSector->GetIdentifier();

	// Reset travel duration
	// TODO intelligent travel remaining duration change
	TravelData.DepartureDate = Game->GetGameWorld()->GetDate();
	GenerateTravelDuration();
}

bool UFlareTravel::CanChangeDestination()
{
	// Travel not possible once started
	return GetElapsedTime() <= 0;
}

void UFlareTravel::GenerateTravelDuration()
{
	TravelDuration = ComputeTravelDuration(Game->GetGameWorld(), OriginSector, DestinationSector, Fleet->GetFleetCompany());
}

int64 UFlareTravel::ComputeTravelDuration(UFlareWorld* World, UFlareSimulatedSector* OriginSector, UFlareSimulatedSector* DestinationSector, UFlareCompany* Company)
{
	int64 TravelDuration = 0;

	if (OriginSector == DestinationSector)
	{
		return 0;
	}

	double OriginAltitude;
	double DestinationAltitude;
	double OriginPhase;
	double DestinationPhase;
	FName OriginCelestialBodyIdentifier;
	FName DestinationCelestialBodyIdentifier;


	OriginAltitude = OriginSector->GetOrbitParameters()->Altitude;
	OriginCelestialBodyIdentifier = OriginSector->GetOrbitParameters()->CelestialBodyIdentifier;
	OriginPhase = OriginSector->GetOrbitParameters()->Phase;

	DestinationAltitude = DestinationSector->GetOrbitParameters()->Altitude;
	DestinationCelestialBodyIdentifier = DestinationSector->GetOrbitParameters()->CelestialBodyIdentifier;
	DestinationPhase = DestinationSector->GetOrbitParameters()->Phase;

	if (OriginCelestialBodyIdentifier == DestinationCelestialBodyIdentifier && OriginAltitude == DestinationAltitude)
	{
		// Phase change travel
		FFlareCelestialBody* CelestialBody = World->GetPlanerarium()->FindCelestialBody(OriginCelestialBodyIdentifier);
		TravelDuration = ComputePhaseTravelDuration(World, CelestialBody, OriginAltitude, OriginPhase, DestinationPhase) / UFlareGameTools::SECONDS_IN_DAY;
	}
	else
	{
		// Altitude change travel
		FFlareCelestialBody* OriginCelestialBody = World->GetPlanerarium()->FindCelestialBody(OriginCelestialBodyIdentifier);
		FFlareCelestialBody* DestinationCelestialBody = World->GetPlanerarium()->FindCelestialBody(DestinationCelestialBodyIdentifier);
		TravelDuration = (UFlareGameTools::SECONDS_IN_DAY/2 + ComputeAltitudeTravelDuration(World, OriginCelestialBody, OriginAltitude, DestinationCelestialBody, DestinationAltitude)) / UFlareGameTools::SECONDS_IN_DAY;
	}

	bool AICheats = World->GetGame()->GetPC()->GetPlayerData()->AICheats;
	if (AICheats)
	{
		int32 GameDifficulty = -1;
		GameDifficulty = World->GetGame()->GetPC()->GetPlayerData()->DifficultyId;
		if (Company == World->GetGame()->GetPC()->GetCompany())
		{
			switch (GameDifficulty)
			{
			case -1: // Easy
				TravelDuration *= 0.95;
				break;
			case 0: // Normal
				break;
			case 1: // Hard
				TravelDuration *= 1.05;
				break;
			case 2: // Very Hard
				TravelDuration *= 1.10;
				break;
			case 3: // Expert
				TravelDuration *= 1.15;
				break;
			case 4: // Unfair
				TravelDuration *= 1.20;
				break;
			}
		}
		else
		{
			switch (GameDifficulty)
			{
			case -1: // Easy
				TravelDuration *= 1.05;
				break;
			case 0: // Normal
				break;
			case 1: // Hard
				TravelDuration *= 0.95;
				break;
			case 2: // Very Hard
				TravelDuration *= 0.90;
				break;
			case 3: // Expert
				TravelDuration *= 0.85;
				break;
			case 4: // Unfair
				TravelDuration *= 0.80;
				break;
			}
		}
	}

	if(Company)//Company->IsTechnologyUnlocked("fast-travel"))
	{

		float TechnologyBonus = Company->IsTechnologyUnlocked("fast-travel") ? 0.5f : 1.f;
		float TechnologyBonusSecondary = Company->GetTechnologyBonus("travel-bonus");
		TechnologyBonus -= TechnologyBonusSecondary;
		TravelDuration *= TechnologyBonus;
//		TravelDuration /= 2;
	}
	return FMath::Max((int64) 2, TravelDuration+1);
}

double UFlareTravel::ComputeSphereOfInfluenceAltitude(UFlareWorld* World, FFlareCelestialBody* CelestialBody)
{
	FFlareCelestialBody* ParentCelestialBody = World->GetPlanerarium()->FindParent(CelestialBody);
	return CelestialBody->OrbitDistance * pow(CelestialBody->Mass / ParentCelestialBody->Mass, 0.4) - CelestialBody->Radius;
}


int64 UFlareTravel::ComputePhaseTravelDuration(UFlareWorld* World, FFlareCelestialBody* CelestialBody, double Altitude, double OriginPhase, double DestinationPhase)
{
	double TravelPhase =  FMath::Abs(FMath::UnwindDegrees(DestinationPhase - OriginPhase));

	double OrbitRadius = CelestialBody->Radius + Altitude;
	double OrbitPerimeter = 2 * PI * OrbitRadius;
	double TravelDistance = OrbitPerimeter * TravelPhase / 360;

	return TRAVEL_DURATION_PER_PHASE_KM * TravelDistance;
}

int64 UFlareTravel::ComputeAltitudeTravelDuration(UFlareWorld* World, FFlareCelestialBody* OriginCelestialBody, double OriginAltitude, FFlareCelestialBody* DestinationCelestialBody, double DestinationAltitude)
{
	double TravelAltitude;

	if (OriginCelestialBody == DestinationCelestialBody)
	{
		TravelAltitude = ComputeAltitudeTravelDistance(World, OriginAltitude, DestinationAltitude);
	}
	else if (World->GetPlanerarium()->IsSatellite(DestinationCelestialBody, OriginCelestialBody))
	{
		// Planet to moon
		TravelAltitude = ComputeAltitudeTravelToMoonDistance(World, OriginCelestialBody, OriginAltitude, DestinationCelestialBody) +
			ComputeAltitudeTravelToSoiDistance(World, DestinationCelestialBody, DestinationAltitude);
	}
	else if (World->GetPlanerarium()->IsSatellite(OriginCelestialBody, DestinationCelestialBody))
	{
		// Moon to planet
		TravelAltitude = ComputeAltitudeTravelToSoiDistance(World, OriginCelestialBody, OriginAltitude) +
				ComputeAltitudeTravelToMoonDistance(World, DestinationCelestialBody, DestinationAltitude, OriginCelestialBody);
	}
	else
	{
		TravelAltitude = ComputeAltitudeTravelToSoiDistance(World, OriginCelestialBody, OriginAltitude) +
				ComputeAltitudeTravelMoonToMoonDistance(World, OriginCelestialBody, DestinationCelestialBody) +
				ComputeAltitudeTravelToSoiDistance(World, DestinationCelestialBody, DestinationAltitude);
	}
	return TRAVEL_DURATION_PER_ALTITUDE_KM * TravelAltitude + UFlareGameTools::SECONDS_IN_DAY;
}

double UFlareTravel::ComputeAltitudeTravelDistance(UFlareWorld* World, double OriginAltitude, double DestinationAltitude)
{
	// Altitude change in same celestial body
	return FMath::Abs(DestinationAltitude - OriginAltitude);
}

double UFlareTravel::ComputeAltitudeTravelToSoiDistance(UFlareWorld* World, FFlareCelestialBody* CelestialBody, double Altitude)
{
	double MoonSoiAltitude = ComputeSphereOfInfluenceAltitude(World, CelestialBody);
	// Travel from moon SOI to moon target altitude
	return FMath::Abs(Altitude - MoonSoiAltitude);
}

double UFlareTravel::ComputeAltitudeTravelToMoonDistance(UFlareWorld* World, FFlareCelestialBody* ParentCelestialBody, double Altitude, FFlareCelestialBody* MoonCelestialBody)
{
	double MoonAltitude = MoonCelestialBody->OrbitDistance - ParentCelestialBody->Radius;
	// Travel to moon altitude
	return FMath::Abs(MoonAltitude - Altitude);
}

double UFlareTravel::ComputeAltitudeTravelMoonToMoonDistance(UFlareWorld* World, FFlareCelestialBody* OriginCelestialBody, FFlareCelestialBody* DestinationCelestialBody)
{
	// Moon1 orbit  to moon2 orbit
	return FMath::Abs(DestinationCelestialBody->OrbitDistance - OriginCelestialBody->OrbitDistance);
}

bool UFlareTravel::IsPlayerHostile()
{
	if(Fleet->GetFleetCompany()->IsPlayerCompany())
	{
		return false;
	}

	bool CanBeHostile = Game->GetQuestManager()->IsUnderMilitaryContract(GetDestinationSector(), Fleet->GetFleetCompany(), false);

	if(!CanBeHostile)
	{
		for (UFlareSimulatedSpacecraft* Ship :Fleet->GetShips())
		{
			if(Game->GetQuestManager()->IsMilitaryTarget(Ship, false))
			{
				CanBeHostile = true;
				break;
			}
		}
	}

	if(!CanBeHostile)
	{
		return false;
	}

	bool HasMilitaryShip = false;
	for (UFlareSimulatedSpacecraft* Ship :Fleet->GetShips())
	{
		if (Ship->IsMilitaryArmed() && !Ship->GetDamageSystem()->IsDisarmed() && !Ship->GetDamageSystem()->IsUncontrollable())
		{
			HasMilitaryShip = true;
			break;
		}
	}

	return HasMilitaryShip;
}

#undef LOCTEXT_NAMESPACE
