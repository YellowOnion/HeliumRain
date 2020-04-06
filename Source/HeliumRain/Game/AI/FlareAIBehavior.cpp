
#include "FlareAIBehavior.h"
#include "../../Flare.h"

#include "../../Data/FlareResourceCatalog.h"

#include "../FlareGame.h"
#include "../FlareCompany.h"
#include "../FlareScenarioTools.h"

#include "../../Quests/FlareQuest.h"
#include "../../Player/FlarePlayerController.h"
#include "../../Spacecrafts/FlareSimulatedSpacecraft.h"


//#define DEBUG_AI_NO_WAR

DECLARE_CYCLE_STAT(TEXT("FlareAIBehavior Load"), STAT_FlareAIBehavior_Load, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("FlareAIBehavior Simulate"), STAT_FlareAIBehavior_Simulate, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("FlareAIBehavior SimulateGeneralBehavior"), STAT_FlareAIBehavior_SimulateGeneralBehavior, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("FlareAIBehavior SimulatePirateBehavior"), STAT_FlareAIBehavior_SimulatePirateBehavior, STATGROUP_Flare);
DECLARE_CYCLE_STAT(TEXT("FlareAIBehavior UpdateDiplomacy"), STAT_FlareAIBehavior_UpdateDiplomacy, STATGROUP_Flare);

#define LOCTEXT_NAMESPACE "UFlareAIBehavior"


/*----------------------------------------------------
	Public API
----------------------------------------------------*/

UFlareAIBehavior::UFlareAIBehavior(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UFlareAIBehavior::Load(UFlareCompany* ParentCompany)
{
	SCOPE_CYCLE_COUNTER(STAT_FlareAIBehavior_Load);

	if(!Company)
	{
		Company = ParentCompany;
		check(Company);
		Game = Company->GetGame();
		ST = Game->GetScenarioTools();
		check(ST);
		GenerateAffilities();
		ProposeTributeToPlayer = false;
	}
}

void UFlareAIBehavior::Simulate(TArray<UFlareCompany*> SortedCompanyValues, TArray<UFlareCompany*> SortedCompanyCombatValues)
{
	SCOPE_CYCLE_COUNTER(STAT_FlareAIBehavior_Simulate);

	// See how the player is doing

//	TArray<UFlareCompany*> SortedCompany = Game->GetGameWorld()->GetCompanies();
//	SortedCompany.Sort(&CompanyValueComparator);

	int32 PlayerCompanyIndex = SortedCompanyValues.IndexOfByKey(GetGame()->GetPC()->GetCompany());
	int32 PlayerArmy = GetGame()->GetPC()->GetCompany()->GetCompanyValue().ArmyCurrentCombatPoints;
	int32 GameDifficulty = GameDifficulty = GetGame()->GetPC()->GetPlayerData()->DifficultyId;

	float PirateRepLoss = -0.5f;
	float RepLossDevisor = 30.f;

	switch (GameDifficulty)
	{
		case -1: // Easy
			PirateRepLoss = -0.25f;
			RepLossDevisor = 32.f;
			break;
		case 0: // Normal
			PirateRepLoss = -0.5f;
			RepLossDevisor = 30.f;
			break;
		case 1: // Hard
			PirateRepLoss = -0.75f;
			RepLossDevisor = 25.f;
			break;
		case 2: // Very Hard
			PirateRepLoss = -1.00f;
			RepLossDevisor = 20.f;
			break;
		case 3: // Expert
			PirateRepLoss = -1.50f;
			RepLossDevisor = 10.f;
			break;
		case 4: // Unfair
			PirateRepLoss = -2.00f;
			RepLossDevisor = 5.f;
			break;
	}

	float ReputationLoss = (PlayerCompanyIndex + 1) / RepLossDevisor;
/*
	GetGame()->GetPC()->Notify(
		LOCTEXT("TestInfo", "Test Notification"),
		FText::Format(
			LOCTEXT("TestInfoFormat", "Rep loss {0}, PlayerCompanyIndex {1}"),
			ReputationLoss, PlayerCompanyIndex),
		"discover-sector",
		EFlareNotification::NT_Info,
		false);*/
	// Pirates hate you

	if (PlayerCompanyIndex > 0 && Company == ST->Pirates)
	{
		Company->GivePlayerReputation(PirateRepLoss);
	}

	// Competitors hate you more if you're doing well
	if (PlayerArmy > 0 && Company != ST->AxisSupplies && Company->GetPlayerReputation() > ReputationLoss)
	{
		Company->GivePlayerReputation(-ReputationLoss);
	}

	// Simulate the day
	if (Company == ST->Pirates)
	{
		SimulatePirateBehavior();
	}
	else
	{
		SimulateGeneralBehavior();
	}

}

void UFlareAIBehavior::SimulateGeneralBehavior()
{
	SCOPE_CYCLE_COUNTER(STAT_FlareAIBehavior_SimulateGeneralBehavior);

	// First make cargo evasion to avoid them to lock themselve trading
	Company->GetAI()->CargosEvasion();

	// Repair and refill ships and stations
	Company->GetAI()->RepairAndRefill();

	Company->GetAI()->ProcessBudget(Company->GetAI()->AllBudgets);

	Company->GetAI()->UpdateMilitaryMovement();
}

void UFlareAIBehavior::UpdateDiplomacy(bool GlobalWar)
{
	SCOPE_CYCLE_COUNTER(STAT_FlareAIBehavior_UpdateDiplomacy);

	// Not global war
	if (GlobalWar)
	{
		return;
	}

	TArray<UFlareCompany*> OtherCompanies = Company->GetOtherCompanies(true); // true for Shuffle
	UFlareCompany* PlayerCompany = GetGame()->GetPC()->GetCompany();

	if(Company->GetAI()->GetData()->Pacifism >= 100)
	{
		// Want peace
		for (UFlareCompany* OtherCompany : OtherCompanies)
		{
			Company->SetHostilityTo(OtherCompany, false);

			TArray<UFlareCompany*> Allies;

			if (Company->GetWarState(OtherCompany) == EFlareHostility::Hostile && Company->GetConfidenceLevel(OtherCompany, Allies) < PayTributeConfidence)
			{
				if (OtherCompany == PlayerCompany)
				{
					ProposeTributeToPlayer = true;
				}
				else
				{
					Company->PayTribute(OtherCompany, false);
				}
			}
		}
	}

	else if (Company->GetAI()->GetData()->Pacifism <= 0)

	{
		//Want war

		if (Company->GetWarCount(PlayerCompany) == 0 )
		{
			// Don't fight if already at war

			// Fill target company sorted by weight (only with weight higther than mine)
			TArray<TPair<UFlareCompany*, float>> TargetCompanies;

			for (UFlareCompany* OtherCompany : OtherCompanies)
			{
				if (Company->IsAtWar(OtherCompany))
				{
					continue;
				}


				//FLOGV("- %s Weight=%f", *OtherCompany->GetCompanyName().ToString(), Weight);
				//FLOGV("-    GetConfidenceLevel=%f", Company->GetConfidenceLevel(OtherCompany));

				if(Company->WantWarWith(OtherCompany))
				{
					//FLOGV("%s add %s as target", *Company->GetCompanyName().ToString(), *OtherCompany->GetCompanyName().ToString());

					TargetCompanies.Add(TPairInitializer<UFlareCompany*, float>(OtherCompany, OtherCompany->ComputeCompanyDiplomaticWeight()));
				}
			}

			TargetCompanies.Sort([&](const TPair<UFlareCompany*, float>& Lhs, const TPair<UFlareCompany*, float>& Rhs){

				if(Lhs.Key == PlayerCompany)
				{
					return true;
				}

				if(Rhs.Key == PlayerCompany)
				{
					return false;
				}

				return Lhs.Value > Rhs.Value;
			});


			auto DeclareWarConfidenceMean = [](TArray<UFlareCompany*> &Allies)
			{
				float DeclareWarConfidenceSum = 0;
				float DeclareWarConfidenceCount = 0;

				for (UFlareCompany* Ally : Allies)
				{
					DeclareWarConfidenceSum += Ally->GetAI()->GetBehavior()->DeclareWarConfidence;
					++DeclareWarConfidenceCount;
				}

				return DeclareWarConfidenceSum / DeclareWarConfidenceCount;
			};

			// For each target
			// find allies list
			// compute allies confidence level
			// compute mean of DeclareWarConfidence
			// if confidente, rand skip else declare war with all allies

			int i = 0;
			for(TPair<UFlareCompany*, float> Target : TargetCompanies)
			{
				if(i >= 3)
				{
					break;
				}
				++i;

				UFlareCompany* TargetCompany = Target.Key;
				float Skipchance = 0.2f;

				if (TargetCompany == PlayerCompany)
				{
					//if looking at player, increase the random chance factor to declare war
					int32 GameDifficulty = GameDifficulty = GetGame()->GetPC()->GetPlayerData()->DifficultyId;

					switch (GameDifficulty)
					{
					case -1: // Easy
						Skipchance = 0.25f;
						break;
					case 0: // Normal
						Skipchance = 0.2f;
						break;
					case 1: // Hard
						Skipchance = 0.18f;
						break;
					case 2: // Very Hard
						Skipchance = 0.16f;
						break;
					case 3: // Expert
						Skipchance = 0.12f;
						break;
					case 4: // Unfair
						Skipchance = 0.08f;
						break;
					}
				}
				else
				{
					//if looking at other companies, decrease the random factor chance for declaring war

					int32 GameDifficulty = GameDifficulty = GetGame()->GetPC()->GetPlayerData()->DifficultyId;

					switch (GameDifficulty)
					{
					case -1: // Easy
						Skipchance = 0.2f;
						break;
					case 0: // Normal
						Skipchance = 0.2f;
						break;
					case 1: // Hard
						Skipchance = 0.30f;
						break;
					case 2: // Very Hard
						Skipchance = 0.40f;
						break;
					case 3: // Expert
						Skipchance = 0.50f;
						break;
					case 4: // Unfair
						Skipchance = 0.60f;
						break;
					}

					//and because reputation is a player only mechanic, we chuck in a diplomacy modifier to the skip chance of the target
					float TechnologyBonus = TargetCompany->IsTechnologyUnlocked("diplomacy") ? 1.5f : 1.f;
					Skipchance *= TechnologyBonus;
				}

				//FLOGV("Target for %s: %s", *Company->GetCompanyName().ToString(), *TargetCompany->GetCompanyName().ToString());
				if(FMath::FRand() <= Skipchance)
				{
					continue;
				}

				TArray<UFlareCompany*> Allies;
				float Confidence = Company->GetConfidenceLevel(TargetCompany, Allies);
				if(Confidence > DeclareWarConfidenceMean(Allies))
				{
					FLOGV("Declare alliance agains %s with:", *TargetCompany->GetCompanyName().ToString());
					for (UFlareCompany* Ally : Allies)
					{
						FLOGV("- %s", *Ally->GetCompanyName().ToString());
						Ally->SetHostilityTo(TargetCompany, true);

					}
				}
			}
		}
	}
}

void UFlareAIBehavior::SimulatePirateBehavior()
{
	SCOPE_CYCLE_COUNTER(STAT_FlareAIBehavior_SimulatePirateBehavior);

	// Repair and refill ships and stations
	Company->GetAI()->RepairAndRefill();

	// First make cargo evasion to avoid them to lock themselve trading
	Company->GetAI()->CargosEvasion();

	Company->GetAI()->ProcessBudget(Company->GetAI()->AllBudgets);

	Company->GetAI()->UpdateMilitaryMovement();

}

void UFlareAIBehavior::GenerateAffilities()
{
	// Reset resource affilities
	ResourceAffilities.Empty();
	ResearchOrder.Empty();
	
	// Default behavior
	SetResourceAffilities(1.f);

	// If above 5, will become exclusive
	SetSectorAffilities(1.f);

	StationCapture = 1.f;
	TradingBuy = 1.f;
	TradingSell = 1.f;
	TradingBoth = 0.5f;

	ShipyardAffility = 1.0;
	ConsumerAffility = 0.5;
	MaintenanceAffility = 0.1;

	// Budget
	BudgetTechnologyWeight = 0.25;
	BudgetMilitaryWeight = 0.5;
	BudgetStationWeight = 1.0;
	BudgetTradeWeight = 1.0;

	ConfidenceTarget = -0.1;
	DeclareWarConfidence = 0.2;
	RequestPeaceConfidence = -0.5;
	PayTributeConfidence = -0.8;

	AttackThreshold = 0.99;
	RetreatThreshold = 0.5;
	DefeatAdaptation = 0.01;

	ArmySize = 5.0;
	DiplomaticReactivity = 1;

	PacifismIncrementRate = 0.8;
	PacifismDecrementRate = 0.6;

	DailyProductionCostSensitivityMilitary = 30;
	DailyProductionCostSensitivityEconomic = 7;
	CostSafetyMarginMilitaryShip = 1.1f;
	CostSafetyMarginTradeShip = 1.05f;
	CostSafetyMarginStation = 1.1f;

	BuildLTradeOnlyTreshhold = 50;
	BuildLMilitaryOnlyTreshhold = 50;

	BuildMilitaryDiversity = 1;
	BuildTradeDiversity = 1;

	// Pirate base
	SetSectorAffility(ST->Boneyard, 0.f);

	UpgradeMilitarySalvagerSRatio = 0.10;
	UpgradeMilitarySalvagerLRatio = 0.10;

	BuildMilitaryDiversitySize = 5;
	BuildTradeDiversitySize = 5;
	BuildMilitaryDiversitySizeBase = 5;
	BuildTradeDiversitySizeBase = 10;
	
	BuildEfficientMilitaryChance = 0.05;
	BuildEfficientMilitaryChanceSmall = 0.10;

	BuildEfficientTradeChance = 0.10;
	BuildEfficientTradeChanceSmall = 0.30;

	if(Company == ST->Pirates)
	{
		// Pirates
		// -------
		//
		// Pirate don't trade, and are only interested in getting money by force
		// All resource affilities are set to 0
		// They never build stations
		// They never research science

		// They have 2 mode : agressive or moderate

		// They need a shipyard, if they have a shipyard they will steal resources,
		// or buy one to build ship, and fill and defend their base
		// They always want graveyard

		// They need some cargo to buy resources for their shipyard and arsenal

		SetResourceAffilities(0.1f);
		SetSectorAffilities(0.f);

		SetSectorAffility(ST->Boneyard, 10.f);

		ShipyardAffility = 0.0;
		ConsumerAffility = 0.0;
		MaintenanceAffility = 0.0;

		// Only buy
		TradingSell = 0.f;
		TradingBoth = 0.f;

		// Doesn't capture station. Change in recovery
		StationCapture = 0.f;

		DeclareWarConfidence = -0.2;
		RequestPeaceConfidence = -0.8;
		PayTributeConfidence = -1.0;

		ArmySize = 50.0;
		AttackThreshold = 0.8;
		RetreatThreshold = 0.2;
		DefeatAdaptation = 0.001;

		// Budget
		BudgetTechnologyWeight = 0.0;
		BudgetMilitaryWeight = 0.5;
		BudgetStationWeight = 0.1;
		BudgetTradeWeight = 0.1;

		PacifismIncrementRate = 1.f;
		PacifismDecrementRate = 0.8f;

		DailyProductionCostSensitivityMilitary = 0;
		DailyProductionCostSensitivityEconomic = 0;

		BuildEfficientMilitaryChance = 0.0125;
		BuildEfficientMilitaryChanceSmall = 0.025;
		UpgradeMilitarySalvagerSRatio = 0.25;
		UpgradeMilitarySalvagerLRatio = 0.25;
		ResearchOrder.Reserve(4);
		ResearchOrder.Add("instruments");
		ResearchOrder.Add("flak");
		ResearchOrder.Add("fast-travel");
		ResearchOrder.Add("bombing");
	}
	else if(Company == ST->GhostWorksShipyards)
	{
		// Loves Hela and doesn't like Nema
		ShipyardAffility = 5.0;

		SetSectorAffilitiesByMoon(ST->Nema,0.5f);
		SetSectorAffilitiesByMoon(ST->Hela, 6.f);

		SetResourceAffility(ST->Steel, 1.5f);
		SetResourceAffility(ST->Plastics, 1.5f);
		SetResourceAffility(ST->Tools, 1.25f);

		// Budget
		BudgetTechnologyWeight = 0.25;
		BudgetMilitaryWeight = 1.0;
		BudgetStationWeight = 1.0;
		BudgetTradeWeight = 1.0;

		BuildMilitaryDiversity = 2;
		BuildTradeDiversity = 2;
		BuildEfficientTradeChance = 0.15;
		ResearchOrder.Add("instruments");
	}
	else if(Company == ST->MiningSyndicate)
	{
		// Mining specialist.
		// Loves raw materials
		// Likes hard work
		SetResourceAffility(ST->Water, 10.f);
		SetResourceAffility(ST->Silica, 10.f);
		SetResourceAffility(ST->IronOxyde, 10.f);
		SetResourceAffility(ST->Hydrogen, 2.f);

		// Budget
		BudgetTechnologyWeight = 0.25;
		BudgetMilitaryWeight = 0.5;
		BudgetStationWeight = 2.0;
		BudgetTradeWeight = 2.0;

		BuildEfficientMilitaryChance = 0.025;
		BuildEfficientMilitaryChanceSmall = 0.5;

		BuildEfficientTradeChance = 0.05;
		BuildEfficientTradeChanceSmall = 0.15;
		ResearchOrder.Reserve(4);
		ResearchOrder.Add("instruments");
		ResearchOrder.Add("orbital-pumps");
		ResearchOrder.Add("metallurgy");
		ResearchOrder.Add("chemicals");

	}
	else if(Company == ST->HelixFoundries)
	{
		// Likes hard factory, and Anka.
		// Base at Outpost
		SetResourceAffility(ST->Steel, 11.f);
		SetResourceAffility(ST->Tools, 10.f);
		SetResourceAffility(ST->Tech, 5.f);
		SetSectorAffilitiesByMoon(ST->Anka, 6.f);
		SetSectorAffility(ST->Outpost, 10.f);
		
		// Budget
		BudgetTechnologyWeight = 0.5;
		BudgetMilitaryWeight = 0.5;
		BudgetStationWeight = 2.0;
		BudgetTradeWeight = 2.0;
		ResearchOrder.Add("instruments");
	}
	else if(Company == ST->Sunwatch)
	{
		// Likes hard factory, and Anka.
		// Base at Outpost
		SetResourceAffility(ST->Fuel, 10.f);
		
		// Budget
		BudgetTechnologyWeight = 0.55;
		BudgetMilitaryWeight = 0.5;
		BudgetStationWeight = 2.0;
		BudgetTradeWeight = 2.0;
		//already starts with instruments
		ResearchOrder.Add("science");
	}
	else if(Company == ST->IonLane)
	{
		CostSafetyMarginMilitaryShip = 1.2f;
		CostSafetyMarginTradeShip = 1.0f;

		BudgetTechnologyWeight = 0.2;
		BudgetMilitaryWeight = 1.0;
		BudgetStationWeight = 0.25;
		BudgetTradeWeight = 2.0;

		ArmySize = 10.0;
		DeclareWarConfidence = 0.1;
		RequestPeaceConfidence = -0.4;
		PayTributeConfidence = -0.85;

		BuildLTradeOnlyTreshhold = 40;
		BuildTradeDiversity = 2;
		BuildTradeDiversitySize = 4;
		BuildEfficientTradeChance = 0.15;
		ResearchOrder.Reserve(2);
		ResearchOrder.Add("instruments");
		ResearchOrder.Add("fast-travel");
	}
	else if(Company == ST->UnitedFarmsChemicals)
	{
		// Likes chemisty
		SetResourceAffility(ST->Food, 10.f);
		SetResourceAffility(ST->Carbon, 5.f);
		SetResourceAffility(ST->Methane, 5.f);

		// Budget
		BudgetTechnologyWeight = 0.5;
		BudgetMilitaryWeight = 0.5;
		BudgetStationWeight = 2.0;
		BudgetTradeWeight = 2.0;

		BuildEfficientMilitaryChance = 0.025;
		BuildEfficientMilitaryChanceSmall = 0.5;

		BuildEfficientTradeChance = 0.05;
		BuildEfficientTradeChanceSmall = 0.15;
		ResearchOrder.Add("instruments");
	}
	else if(Company == ST->NemaHeavyWorks)
	{
		// Likes Nema and heavy work
		SetResourceAffility(ST->FleetSupply, 2.f);
		SetResourceAffility(ST->Steel, 6.f);
		SetResourceAffility(ST->Tools, 5.f);
		SetResourceAffility(ST->Tech, 5.f);
		SetSectorAffilitiesByMoon(ST->Nema, 5.f);

		ShipyardAffility = 3.0;

		// Budget
		BudgetTechnologyWeight = 0.5;
		BudgetMilitaryWeight = 0.5;
		BudgetStationWeight = 2.0;
		BudgetTradeWeight = 2.0;

		BuildMilitaryDiversity = 2;
		BuildTradeDiversity = 2;

		BuildEfficientTradeChance = 0.15;
		ResearchOrder.Reserve(3);
		ResearchOrder.Add("instruments");
		ResearchOrder.Add("science");
		ResearchOrder.Add("shipyard-station");
	}
	else if(Company == ST->AxisSupplies)
	{
		// Assures fleet supply availability
		SetResourceAffility(ST->FleetSupply, 5.f);
		SetResourceAffility(ST->Food, 2.f);

		ShipyardAffility = 0.0;
		ConsumerAffility = 1.0;
		MaintenanceAffility = 10.0;

		CostSafetyMarginMilitaryShip = 1.2f;
		CostSafetyMarginTradeShip = 1.0f;

		// Budget
		BudgetTechnologyWeight = 0.25;
		BudgetMilitaryWeight = 0.30;
		BudgetStationWeight = 2.0;
		BudgetTradeWeight = 2.0;

		ArmySize = 1.0;
		DeclareWarConfidence = 1.0;
		RequestPeaceConfidence = 0.0;
		PayTributeConfidence = -0.1;

		DiplomaticReactivity = 0.1;
		BuildLTradeOnlyTreshhold = 40;
		BuildTradeDiversitySize = 4;

		BuildEfficientTradeChance = 0.15;
		ResearchOrder.Reserve(2);
		ResearchOrder.Add("instruments");
		ResearchOrder.Add("fast-travel");
	}
	else if (Company == ST->BrokenMoon)
	{
		DailyProductionCostSensitivityMilitary = 7;
		DailyProductionCostSensitivityEconomic = 30;
		BuildLMilitaryOnlyTreshhold = 40;

		BuildMilitaryDiversity = 2;
		BuildMilitaryDiversitySize = 4;
		BuildMilitaryDiversitySizeBase = 0;

		BuildEfficientMilitaryChance = 0.10;
		BuildEfficientMilitaryChanceSmall = 0.15;
		ResearchOrder.Reserve(4);
		ResearchOrder.Add("instruments");
		ResearchOrder.Add("flak");
		ResearchOrder.Add("bombing");
		ResearchOrder.Add("pirate-tech");

	}
	else if (Company == ST->Quantalium)
	{
		SetResourceAffility(ST->Tech, 10.f);

		BudgetTechnologyWeight = 0.8;
		BudgetMilitaryWeight = 0.5;
		BudgetStationWeight = 1.0;
		BudgetTradeWeight = 1.0;

		BuildEfficientMilitaryChance = 0.10;
		BuildEfficientMilitaryChanceSmall = 0.15;

		BuildEfficientTradeChance = 0.20;
		BuildEfficientTradeChanceSmall = 0.30;
		ResearchOrder.Reserve(3);
		ResearchOrder.Add("instruments");
		ResearchOrder.Add("science");
		ResearchOrder.Add("fast-travel");
	}
	else
	{
	ResearchOrder.Add("instruments");
	}
}

void UFlareAIBehavior::SetResourceAffilities(float Value)
{
	for(int32 ResourceIndex = 0; ResourceIndex < Game->GetResourceCatalog()->Resources.Num(); ResourceIndex++)
	{
		FFlareResourceDescription* Resource = &Game->GetResourceCatalog()->Resources[ResourceIndex]->Data;
		SetResourceAffility(Resource, Value);
	}
}

void UFlareAIBehavior::SetResourceAffility(FFlareResourceDescription* Resource, float Value)
{
	if(ResourceAffilities.Contains(Resource)){
		ResourceAffilities[Resource] = Value;
	}
	else
	{
		ResourceAffilities.Add(Resource, Value);
	}
}


void UFlareAIBehavior::SetSectorAffilities(float Value)
{
	for(int32 SectorIndex = 0; SectorIndex < Game->GetGameWorld()->GetSectors().Num(); SectorIndex++)
	{
		UFlareSimulatedSector* Sector = Game->GetGameWorld()->GetSectors()[SectorIndex];
		SetSectorAffility(Sector, Value);
	}
}

void UFlareAIBehavior::SetSectorAffility(UFlareSimulatedSector* Sector, float Value)
{
	if(SectorAffilities.Contains(Sector)){
		SectorAffilities[Sector] = Value;
	}
	else
	{
		SectorAffilities.Add(Sector, Value);
	}
}

void UFlareAIBehavior::SetSectorAffilitiesByMoon(FFlareCelestialBody *CelestialBody, float Value)
{
	for(int32 SectorIndex = 0; SectorIndex < Game->GetGameWorld()->GetSectors().Num(); SectorIndex++)
	{
		UFlareSimulatedSector* Sector = Game->GetGameWorld()->GetSectors()[SectorIndex];

		if(Sector->GetOrbitParameters()->CelestialBodyIdentifier == CelestialBody->Identifier)
		{
			SetSectorAffility(Sector, Value);
		}
	}
}


/*----------------------------------------------------
	Getters
----------------------------------------------------*/

float UFlareAIBehavior::GetBudgetWeight(EFlareBudget::Type Budget)
{
	switch (Budget) {
	case EFlareBudget::Military:
		return (Company->AtWar() ? BudgetMilitaryWeight * 10 : BudgetMilitaryWeight);
		break;
	case EFlareBudget::Trade:
		return BudgetTradeWeight;
		break;
	case EFlareBudget::Station:
		return BudgetStationWeight;
		break;
	case EFlareBudget::Technology:
		return BudgetTechnologyWeight;
		break;
	default:
		break;
	}

	return 0;
}

float UFlareAIBehavior::GetSectorAffility(UFlareSimulatedSector* Sector)
{
	if(SectorAffilities.Contains(Sector))
	{
		return SectorAffilities[Sector];
	}
	return 1.f;
}

float UFlareAIBehavior::GetResourceAffility(FFlareResourceDescription* Resource)
{
	if(ResourceAffilities.Contains(Resource))
	{
		return ResourceAffilities[Resource];
	}
	return 1.f;
}

float UFlareAIBehavior::GetAttackThreshold()
{
	return AttackThreshold + Company->GetAI()->GetData()->Caution;
}

#undef LOCTEXT_NAMESPACE
