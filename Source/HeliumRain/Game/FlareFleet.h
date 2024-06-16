#pragma once

#include "Object.h"
#include "../Data/FlareResourceCatalogEntry.h"
#include "../Spacecrafts/FlareSpacecraftTypes.h"
#include "FlareFleet.generated.h"


class UFlareSimulatedSector;
class UFlareSimulatedSpacecraft;
class AFlareGame;
class AFlareBomb;
class UFlareCompany;
class UFlareTravel;
class UFlareTradeRoute;
class UFlareCompanyWhiteList;
struct FFlareSpacecraftSave;

/** Fleet save data */
USTRUCT()
struct FFlareFleetSave
{
	GENERATED_USTRUCT_BODY()

	/** Given Name */
	UPROPERTY(EditAnywhere, Category = Save)
	FText Name;

	/** Fleet identifier */
	UPROPERTY(EditAnywhere, Category = Save)
	FName Identifier;

	/** Fleet ships */
	UPROPERTY(VisibleAnywhere, Category = Save)
	TArray<FName> ShipImmatriculations;

	/** Fleet color */
	UPROPERTY(EditAnywhere, Category = Save)
	FLinearColor FleetColor;

	/** Fleet use autotrade */
	UPROPERTY(EditAnywhere, Category = Save)
	bool AutoTrade;

	/** Fleet hide from sector travel list */
	UPROPERTY(EditAnywhere, Category = Save)
	bool HideTravelList;

	/** Days since last stats reset */
	UPROPERTY(EditAnywhere, Category = Save)
	int32 AutoTradeStatsDays;

	/** Load resource count since last stats reset */
	UPROPERTY(EditAnywhere, Category = Save)
	int32 AutoTradeStatsLoadResources;

	/** Unload resource cout since last stats reset */
	UPROPERTY(EditAnywhere, Category = Save)
	int32 AutoTradeStatsUnloadResources;

	/** Amount of money gain since last stats reset */
	UPROPERTY(EditAnywhere, Category = Save)
	int64 AutoTradeStatsMoneySell;

	/** Amount of money spent since last stats reset */
	UPROPERTY(EditAnywhere, Category = Save)
	int64 AutoTradeStatsMoneyBuy;

	/* Whitelist Identifier*/
	UPROPERTY(EditAnywhere, Category = Save)
	FName DefaultWhiteListIdentifier;
};

UCLASS()
class HELIUMRAIN_API UFlareFleet : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/*----------------------------------------------------
	  Save
	----------------------------------------------------*/

	/** Load the fleet from a save file */
	virtual void Load(const FFlareFleetSave& Data);

	/** Save the fleet to a save file */
	virtual FFlareFleetSave* Save();


	/*----------------------------------------------------
		Gameplay
	----------------------------------------------------*/

	/** Get a name for this fleet (capital ship's name, etc) */
	FText GetName();

	/** Tell us if we can add ship, and why */
	virtual bool CanAddShip(UFlareSimulatedSpacecraft* Ship);

	virtual void AddShip(UFlareSimulatedSpacecraft* Ship);

	int32 InterceptShips();

	virtual void RemoveShip(UFlareSimulatedSpacecraft* Ship, bool destroyed = false, bool reformfleet = true);

	/**Remove multiple ships simultaniously, moving them all to the same fleet*/
	virtual void RemoveShips(TArray<UFlareSimulatedSpacecraft*> ShipsToRemove);

	/** Remove all ship from the fleet and delete it. Not possible during travel */
	virtual void Disband();

	/** Tell us if we can merge, and why */
	virtual bool CanMerge(UFlareFleet* Fleet, FText& OutInfo);

	/** Tell us if we are travelling */
	bool IsTraveling() const;

	bool IsTrading() const;

	//is at least one ship in the fleet alive?
	bool IsAlive();


	/** Tell us if we can travel */
	bool CanTravel(UFlareSimulatedSector* TargetSector = nullptr);

	/** Tell us if we can travel, and why */
	bool CanTravel(FText& OutInfo, UFlareSimulatedSector* TargetSector = nullptr);

	virtual void Merge(UFlareFleet* Fleet);

	virtual void SetCurrentSector(UFlareSimulatedSector* Sector);

	void SetCurrentTravel(UFlareTravel* Travel);

	virtual void SetCurrentTradeRoute(UFlareTradeRoute* TradeRoute)
	{
		CurrentTradeRoute = TradeRoute;
		if(CurrentTradeRoute != nullptr)
		{
			SetAutoTrading(false);
		}
	}

	void SetAutoTrading(bool Autotrading)
	{
		FleetData.AutoTrade = Autotrading;
		FleetData.AutoTradeStatsDays = 0;
		FleetData.AutoTradeStatsLoadResources = 0;
		FleetData.AutoTradeStatsUnloadResources = 0;
		FleetData.AutoTradeStatsMoneySell = 0;
		FleetData.AutoTradeStatsMoneyBuy = 0;
	}

	void SetHideTravel(bool NewHideTravel)
	{
		FleetData.HideTravelList = NewHideTravel;
	}
	
	virtual void InitShipList();

	void RemoveImmobilizedShips();

	void SetFleetColor(FLinearColor Color);

	FLinearColor GetFleetColor() const;

	int32 GetRepairDuration() const;

	int32 GetRefillDuration() const;
	FText GetTravelConfirmText();

	void SelectWhiteListDefault(FName IdentifierSearch);
	void SelectWhiteListDefault(UFlareCompanyWhiteList* NewWhiteList);

	UFlareCompanyWhiteList* GetActiveWhitelist();
	bool CanTradeWhiteListFrom(UFlareSimulatedSpacecraft* OtherSpacecraft, FFlareResourceDescription* Resource);
	bool CanTradeWhiteListTo(UFlareSimulatedSpacecraft* OtherSpacecraft, FFlareResourceDescription* Resource);

protected:

	TArray<UFlareSimulatedSpacecraft*>     FleetShips;

	UFlareCompany*			               FleetCompany;
	FFlareFleetSave                        FleetData;
	AFlareGame*                            Game;
	bool                                   IsShipListLoaded;
	UFlareSimulatedSector*                 CurrentSector;
	UFlareTravel*                          CurrentTravel;
	UFlareTradeRoute*                      CurrentTradeRoute;
	uint32								   FleetCount;
	TArray<AFlareBomb*>					   IncomingBombs;
	int32								   UnableToTravelShips;

	UFlareCompanyWhiteList*				   FleetSelectedWhiteList;

public:
	/*----------------------------------------------------
		Getters
	----------------------------------------------------*/

	int32 GetUnableToTravelShips() const;

	void TrackIncomingBomb(AFlareBomb* Bomb);
	void UnTrackIncomingBomb(AFlareBomb* Bomb);
	TArray<AFlareBomb*> GetIncomingBombs();

	AFlareGame* GetGame() const
	{
		return Game;
	}

	inline UFlareCompanyWhiteList* GetSelectedWhiteList() const
	{
		return FleetSelectedWhiteList;
	}

	FText GetFleetName() const;

	void SetFleetName(FText Name)
	{
		FleetData.Name = Name;
	}

	UFlareCompany* GetFleetCompany() const
	{
		return FleetCompany;
	}

	FName GetIdentifier() const
	{
		return FleetData.Identifier;
	}

	bool IsAutoTrading() const
	{
		return FleetData.AutoTrade;
	}

	bool IsHiddenTravel() const
	{
		return FleetData.HideTravelList;
	}

	FFlareFleetSave* GetData()
	{
		return &FleetData;
	}

	TArray<UFlareSimulatedSpacecraft*>& GetShips();

	uint32 GetImmobilizedShipCount();

	uint32 GetTradingShipCount() const;

	/** Get the current ship count in the fleet */
	uint32 GetShipCount() const;

	/** Get the current military ship count by size in the fleet */
	uint32 GetMilitaryShipCountBySize(EFlarePartSize::Type Size) const;

	/** Get the maximum ship count in a fleet */
	uint32 GetMaxShipCount();

	/** Get information about the current travel, if any */
	FText GetStatusInfo() const;

	/** Return null if traveling */
	inline UFlareSimulatedSector* GetCurrentSector() const
	{
		return CurrentSector;
	}

	/** Return null if not traveling */
	inline UFlareTravel* GetCurrentTravel() const
	{
		return CurrentTravel;
	}

	/** Return null if not in a trade route */
	inline UFlareTradeRoute* GetCurrentTradeRoute() const
	{
		return CurrentTradeRoute;
	}

	int32 GetFleetCapacity(bool SkipIfStranded = false) const;
	int32 GetFleetUsedCargoSpace() const;
	int32 GetFleetFreeCargoSpace() const;
	int32 GetFleetResourceQuantity(FFlareResourceDescription* Resource);
	int32 GetFleetFreeSpaceForResource(FFlareResourceDescription* Resource);

	int32 GetCombatPoints(bool ReduceByDamage) const;

	bool IsRepairing() const;
	bool IsRefilling() const;
	bool FleetNeedsRepair() const;
	bool FleetNeedsRefill() const;
};