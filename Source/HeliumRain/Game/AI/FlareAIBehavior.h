#pragma once

#include "Object.h"
#include "../FlareGameTypes.h"
#include "FlareAIBehavior.generated.h"


class UFlareCompany;
class UFlareScenarioTools;
struct FFlareCelestialBody;

UCLASS()
class HELIUMRAIN_API UFlareAIBehavior : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/*----------------------------------------------------
		Public API
	----------------------------------------------------*/

	/** Load the company behavior */
	virtual void Load(UFlareCompany* ParentCompany);

	/** Load the company behavior */
	virtual void Load(FFlareCompanyDescription* Description,AFlareGame* LoadingGame);

	virtual void Simulate();

	void UpdateDiplomacy(bool GlobalWar);

protected:

	/*----------------------------------------------------
		Internal subsystems
	----------------------------------------------------*/

	virtual void SimulateGeneralBehavior();

	virtual void SimulatePirateBehavior();

	void GenerateAffilities(bool Basic = false);

	/*----------------------------------------------------
		Helpers
	----------------------------------------------------*/


	void SetResourceAffilities(float Value);

	void SetResourceAffility(FFlareResourceDescription* Resource, float Value);

	void SetSectorAffilities(float Value);

	void SetSectorAffility(UFlareSimulatedSector* Sector, float Value);

	void SetSectorAffilitiesByMoon(FFlareCelestialBody *CelestialBody, float Value);

protected:

	/*----------------------------------------------------
		Data
	----------------------------------------------------*/

	// Gameplay data
	UFlareCompany*			               Company;
	FFlareCompanyDescription*			   CompanyDescription;
	AFlareGame*                            Game;
	UFlareScenarioTools*                   ST;

	TMap<FFlareResourceDescription*, float> ResourceAffilities;
	TMap<UFlareSimulatedSector*, float>		SectorAffilities;

public:

	/*----------------------------------------------------
		Public Data
	----------------------------------------------------*/

	float StationCapture;
	float TradingBuy;
	float TradingSell;
	float TradingBoth;
	float ShipyardAffility;
	float ConsumerAffility;
	float MaintenanceAffility;
	float BudgetTechnologyWeight;
	float BudgetMilitaryWeight;
	float BudgetStationWeight;
	float BudgetTradeWeight;
	float ArmySize;

	float AttackThreshold;
	float RetreatThreshold;

	/* Amount of caution gain at each defeat of loose at each victory */
	float DefeatAdaptation;

	float ConfidenceTarget;
	float DeclareWarConfidence;
	float RequestPeaceConfidence;
	float PayTributeConfidence;

	float DiplomaticReactivity;

	float PacifismIncrementRate;
	float PacifismDecrementRate;

	float DailyProductionCostSensitivityMilitary;
	float DailyProductionCostSensitivityEconomic;

	float CostSafetyMarginMilitaryShip;
	float CostSafetyMarginTradeShip;
	float CostSafetyMarginStation;

	//once past this level of ships switch to only building L sized
	int32 BuildLTradeOnlyTreshhold;
	int32 BuildLMilitaryOnlyTreshhold;

	int32 BuildMilitaryDiversity;
	int32 BuildTradeDiversity;

	int32 BuildMilitaryDiversitySize;
	int32 BuildTradeDiversitySize;
	int32 BuildMilitaryDiversitySizeBase;
	int32 BuildTradeDiversitySizeBase;

	float BuildEfficientMilitaryChance;
	float BuildEfficientTradeChance;
	float BuildEfficientMilitaryChanceSmall;
	float BuildEfficientTradeChanceSmall;

	float UpgradeMilitarySalvagerSRatio;
	float UpgradeMilitarySalvagerLRatio;

	bool ProposeTributeToPlayer = false;
	bool FinishedResearch = false;
	TArray<FName> ResearchOrder;

	/*----------------------------------------------------
		Getters
	----------------------------------------------------*/

	AFlareGame* GetGame() const
	{
		return Game;
	}

	float GetBudgetWeight(EFlareBudget::Type Budget);

	float GetSectorAffility(UFlareSimulatedSector* Sector);
	float GetResourceAffility(FFlareResourceDescription* Resource);

	float GetAttackThreshold();
};