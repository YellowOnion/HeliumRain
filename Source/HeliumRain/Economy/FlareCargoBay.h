#pragma once

#include "../Spacecrafts/FlareSpacecraftTypes.h"
#include "FlareResource.h"
#include "FlareCargoBay.generated.h"


struct FFlareResourceDescription;
//class AFlarePlayerController;
class AFlareGame;
class UFlareCompany;
class UFlareSimulatedSpacecraft;

struct FSortableCargoInfo
{
	FFlareCargo*    Cargo;
	int32           CargoInitialIndex;
};


UCLASS()
class HELIUMRAIN_API UFlareCargoBay : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/*----------------------------------------------------
	   Save
	----------------------------------------------------*/

	/** Load the factory from a save file */
	virtual void Load(UFlareSimulatedSpacecraft* ParentSpacecraft, TArray<FFlareCargoSave>& Data, int32 minCargoBayCount, int32 minCargoBaySlotCapacity);

	/** Save the factory to a save file */
	virtual TArray<FFlareCargoSave>* Save();


	/*----------------------------------------------------
	   Gameplay
	----------------------------------------------------*/

	/* If client is not null, restriction are used*/
	bool HasResources(FFlareResourceDescription* Resource, int32 Quantity, UFlareCompany* Client, uint8 CheckRestrictionContext = 0, bool IgnoresRestrictionNobody = false);

	/* If client is not null, restriction are used*/
	int32 TakeResources(FFlareResourceDescription* Resource, int32 Quantity, UFlareCompany* Client, uint8 CheckRestrictionContext = 0, bool IgnoresRestrictionNobody = false);

	void DumpCargo(FFlareCargo* Cargo);

	/* If client is not null, restriction are used*/
	int32 GiveResources(FFlareResourceDescription* Resource, int32 Quantity, UFlareCompany* Client, uint8 CheckRestrictionContext = 0, bool IgnoresRestrictionNobody = false);

	void UnlockAll(bool IgnoreManualLock = true);

	void HideUnlockedSlots();

	bool LockSlot(FFlareResourceDescription* Resource, EFlareResourceLock::Type LockType, bool ManualLock);

	TEnumAsByte<EFlareResourceRestriction::Type> RotateSlotRestriction(int32 SlotIndex);

	TEnumAsByte<EFlareResourceRestriction::Type> GetRestriction(int32 SlotIndex);

	void SetSlotRestriction(int32 SlotIndex, EFlareResourceRestriction::Type RestrictionType);

protected:

	/*----------------------------------------------------
	   Protected data
	----------------------------------------------------*/

	// Gameplay data
	TArray<FFlareCargoSave>                    CargoBayData;
	UFlareSimulatedSpacecraft*				   Parent;

	TArray<FFlareCargo>                        CargoBay;

	// Cache
	int32								       CargoBayCount;
	int32								       CargoBaySlotCapacity;

public:
	AFlareGame*                                Game;

public:

	/*----------------------------------------------------
		Getters
	----------------------------------------------------*/

	int32 GetSlotCount() const;

	int32 GetCapacity() const;

	int32 GetFreeSlotCount() const;

	int32 GetUsedCargoSpace() const;

	int32 GetFreeCargoSpace() const;

	/* If client is not null, consider as not identified company*/
	int32 GetResourceQuantity(FFlareResourceDescription* Resource, UFlareCompany* Client, uint8 CheckRestrictionContext = 0, bool IgnoresRestrictionNobody = false) const;

	/* Quantity only, simple method*/
	int32 GetResourceQuantitySimple(FFlareResourceDescription* Resource) const;

	

	/* If client is not null, consider as not identified company*/
	int32 GetFreeSpaceForResource(FFlareResourceDescription* Resource, UFlareCompany* Client, bool LockOnly = false, uint8 Context = 2, bool IgnoresRestrictionNobody = false) const;

	int32 GetTotalCapacityForResource(FFlareResourceDescription* Resource, UFlareCompany* Client, bool LockOnly = false) const;


	bool HasRestrictions() const;

	FFlareCargo* GetSlot(int32 Index);

	TArray<FFlareCargo>& GetSlots()
	{
		return CargoBay;
	}

	int32 GetSlotCapacity() const;

	inline UFlareSimulatedSpacecraft* GetParent() const
	{
		return Parent;
	}

	/* If client is not null, consider as not identified company*/
	bool WantSell(FFlareResourceDescription* Resource, UFlareCompany* Client, bool RequireStock = false) const;

	/* If client is not null, consider as not identified company*/
	bool WantBuy(FFlareResourceDescription* Resource, UFlareCompany* Client) const;

	bool CheckRestriction(const FFlareCargo* Cargo, UFlareCompany* Client, uint8 Context = 0, bool IgnoresRestrictionNobody = false) const;

	static bool SortBySlotType(const FSortableCargoInfo& A, const FSortableCargoInfo& B);

};

