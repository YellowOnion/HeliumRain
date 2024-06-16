
#include "FlareCargoBay.h"
#include "../Flare.h"

#include "FlareFactory.h"

#include "../Data/FlareResourceCatalog.h"

#include "../Game/FlareGame.h"
#include "../Quests/FlareQuestManager.h"

#include "../Spacecrafts/FlareSimulatedSpacecraft.h"

//#include "../Player/FlarePlayerController.h"

/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

#define LOCTEXT_NAMESPACE "FlareCargoBay"

UFlareCargoBay::UFlareCargoBay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UFlareCargoBay::Load(UFlareSimulatedSpacecraft* ParentSpacecraft, TArray<FFlareCargoSave>& Data, int32 minCargoBayCount, int32 minCargoBaySlotCapacity)
{
	Parent = ParentSpacecraft;
	Game = Parent->GetGame();

	CargoBayCount = minCargoBayCount;
	CargoBaySlotCapacity = minCargoBaySlotCapacity;




	int32 UsedBay = 0;

	for(FFlareCargoSave const& Cargo: Data)
	{
		if(Cargo.Quantity > 0 || Cargo.Lock != EFlareResourceLock::NoLock)
		{
			++UsedBay;
		}

		if(Cargo.Quantity > CargoBaySlotCapacity)
		{
			CargoBaySlotCapacity = Cargo.Quantity;
		}
	}


	if(UsedBay > CargoBayCount)
	{
		CargoBayCount = UsedBay;
	}

	TArray<int32> UsedIndexes;

	if(Data.Num() > CargoBayCount)
	{
		int32 Index = 0;
		for(FFlareCargoSave const& Cargo: Data)
		{
			if(Cargo.Quantity > 0 || Cargo.Lock != EFlareResourceLock::NoLock)
			{
				UsedIndexes.Push(Index);
			}
			++Index;
		}
	}
	else
	{
		int32 Index = 0;
		for(FFlareCargoSave const& Cargo: Data)
		{
			UsedIndexes.Push(Index);
			++Index;
		}
	}

	// Initialize cargo bay
	CargoBay.Empty();
	for (int32 CargoIndex = 0; CargoIndex < CargoBayCount; CargoIndex++)
	{
		FFlareCargo Cargo;
		Cargo.Resource = NULL;
		Cargo.Quantity = 0;
		Cargo.Lock = EFlareResourceLock::NoLock;
		Cargo.Restriction = EFlareResourceRestriction::Everybody;
		Cargo.ManualLock= false;

		if (CargoIndex < UsedIndexes.Num())
		{
			// Existing save
			FFlareCargoSave* CargoSave = &Data[UsedIndexes[CargoIndex]];
			Cargo.Restriction = CargoSave->Restriction;

			if (CargoSave->Quantity > 0)
			{
				Cargo.Resource = Game->GetResourceCatalog()->Get(CargoSave->ResourceIdentifier);
				Cargo.Quantity = FMath::Min(CargoSave->Quantity, GetSlotCapacity());
			}

			if(CargoSave->Lock != EFlareResourceLock::NoLock)
			{
				Cargo.Resource = Game->GetResourceCatalog()->Get(CargoSave->ResourceIdentifier);
				Cargo.Lock = CargoSave->Lock;
				Cargo.ManualLock = true;
			}
		}

		CargoBay.Add(Cargo);
	}
}


TArray<FFlareCargoSave>* UFlareCargoBay::Save()
{
	CargoBayData.Empty();
	for (int CargoIndex = 0; CargoIndex < CargoBay.Num() ; CargoIndex++)
	{
		FFlareCargo& Cargo = CargoBay[CargoIndex];
		FFlareCargoSave CargoSave;
		CargoSave.Quantity = Cargo.Quantity;
		if (Cargo.Resource != NULL)
		{
			CargoSave.ResourceIdentifier = Cargo.Resource->Identifier;
		}
		else
		{
			CargoSave.ResourceIdentifier = NAME_None;
		}
		if(Cargo.ManualLock && Cargo.Lock != 255)
		{
			CargoSave.Lock = Cargo.Lock;
		}
		else
		{
			CargoSave.Lock = EFlareResourceLock::NoLock;
		}

		CargoSave.Restriction = Cargo.Restriction;

		CargoBayData.Add(CargoSave);
	}

	return &CargoBayData;
}


/*----------------------------------------------------
   Gameplay
----------------------------------------------------*/

bool UFlareCargoBay::HasResources(FFlareResourceDescription* Resource, int32 Quantity, UFlareCompany* Client, uint8 CheckRestrictionContext,bool IgnoresRestrictionNobody)
{
	int32 PresentQuantity = 0;

	if (Quantity == 0)
	{
		return true;
	}

	for (int CargoIndex = 0; CargoIndex < CargoBay.Num() ; CargoIndex++)
	{
		FFlareCargo* Cargo = &CargoBay[CargoIndex];

		if(!CheckRestriction(Cargo, Client, CheckRestrictionContext, IgnoresRestrictionNobody))
		{
			continue;
		}

		if (Cargo->Resource == Resource)
		{
			PresentQuantity += Cargo->Quantity;
			if (PresentQuantity >= Quantity)
			{
				return true;
			}
		}
	}
	return false;
}

int32 UFlareCargoBay::TakeResources(FFlareResourceDescription* Resource, int32 Quantity, UFlareCompany* Client, uint8 CheckRestrictionContext, bool IgnoresRestrictionNobody)
{
	int32 QuantityToTake = Quantity;


	if (QuantityToTake == 0)
	{
		return 0;
	}

	// First pass: take resource from the less full cargo
	int32 MinQuantity = 0;
	FFlareCargo* MinQuantityCargo = NULL;

	for (int CargoIndex = 0; CargoIndex < CargoBay.Num() ; CargoIndex++)
	{
		FFlareCargo& Cargo = CargoBay[CargoIndex];
		if (Cargo.Resource == Resource)
		{
			if(!CheckRestriction(&Cargo, Client, CheckRestrictionContext, IgnoresRestrictionNobody))
			{
				continue;
			}

			if (MinQuantityCargo == NULL || MinQuantity > Cargo.Quantity)
			{
				MinQuantityCargo = &Cargo;
				MinQuantity = Cargo.Quantity;
			}
		}
	}

	if (MinQuantityCargo)
	{
		int32 TakenQuantity = FMath::Min(MinQuantityCargo->Quantity, QuantityToTake);
		if (TakenQuantity > 0)
		{
			MinQuantityCargo->Quantity -= TakenQuantity;
			QuantityToTake -= TakenQuantity;

			if (MinQuantityCargo->Quantity == 0 && MinQuantityCargo->Lock == EFlareResourceLock::NoLock)
			{
				MinQuantityCargo->Resource = NULL;
			}

			if (QuantityToTake == 0)
			{
				return Quantity;
			}
		}
	}


	for (int CargoIndex = 0; CargoIndex < CargoBay.Num() ; CargoIndex++)
	{
		FFlareCargo& Cargo = CargoBay[CargoIndex];
		if (Cargo.Resource == Resource)
		{
			if(!CheckRestriction(&Cargo, Client, CheckRestrictionContext, IgnoresRestrictionNobody))
			{
				continue;
			}

			int32 TakenQuantity = FMath::Min(Cargo.Quantity, QuantityToTake);
			if (TakenQuantity > 0)
			{
				Cargo.Quantity -= TakenQuantity;
				QuantityToTake -= TakenQuantity;

				if (Cargo.Quantity == 0 && Cargo.Lock == EFlareResourceLock::NoLock)
				{
					Cargo.Resource = NULL;
				}

				if (QuantityToTake == 0)
				{
					return Quantity;
				}
			}
		}
	}
	return Quantity - QuantityToTake;
}

void UFlareCargoBay::DumpCargo(FFlareCargo* Cargo)
{
	Cargo->Quantity = 0;
	if (Cargo->Lock == EFlareResourceLock::NoLock)
	{
		Cargo->Resource = NULL;
	}
}

int32 UFlareCargoBay::GiveResources(FFlareResourceDescription* Resource, int32 Quantity, UFlareCompany* Client, uint8 CheckRestrictionContext, bool IgnoresRestrictionNobody)
{
	int32 QuantityToGive = Quantity;

	if (QuantityToGive == 0)
	{
		return Quantity;
	}

	// First pass, fill already existing slots
	for (int CargoIndex = 0 ; CargoIndex < CargoBay.Num() ; CargoIndex++)
	{
		FFlareCargo& Cargo = CargoBay[CargoIndex];
		if (Resource == Cargo.Resource)
		{
			if(!CheckRestriction(&Cargo, Client, CheckRestrictionContext, IgnoresRestrictionNobody))
			{
				continue;
			}

			// Same resource
			int32 AvailableCapacity = GetSlotCapacity() - Cargo.Quantity;
			int32 GivenQuantity = FMath::Min(AvailableCapacity, QuantityToGive);
			if (GivenQuantity > 0)
			{
				Cargo.Quantity += GivenQuantity;
				QuantityToGive -= GivenQuantity;

				if (QuantityToGive == 0)
				{
					return Quantity;
				}
			}
		}
	}

	// Fill free cargo slots
	for (int CargoIndex = 0 ; CargoIndex < CargoBay.Num() ; CargoIndex++)
	{
		FFlareCargo& Cargo = CargoBay[CargoIndex];
		if (Cargo.Resource == NULL)
		{
			if(!CheckRestriction(&Cargo, Client, CheckRestrictionContext, IgnoresRestrictionNobody))
			{
				continue;
			}

			// Empty Cargo
			int32 GivenQuantity = FMath::Min(GetSlotCapacity(), QuantityToGive);
			if (GivenQuantity > 0)
			{
				Cargo.Quantity += GivenQuantity;
				Cargo.Resource = Resource;

				QuantityToGive -= GivenQuantity;

				if (QuantityToGive == 0)
				{
					return Quantity;
				}
			}
			else
			{
				FLOGV("Zero sized cargo bay for %s", *Parent->GetImmatriculation().ToString())
			}

		}
	}

	return Quantity - QuantityToGive;
}


/*----------------------------------------------------
	Getters
----------------------------------------------------*/

int32 UFlareCargoBay::GetSlotCapacity() const
{
	return CargoBaySlotCapacity;
}

int32 UFlareCargoBay::GetCapacity() const
{
	return GetSlotCapacity() * CargoBayCount;
}

int32 UFlareCargoBay::GetFreeSlotCount() const
{
	int32 FreeSlotCount = 0;

	for (int CargoIndex = 0; CargoIndex < CargoBay.Num(); CargoIndex++)
	{
		if(CargoBay[CargoIndex].Quantity == 0)
		{
			FreeSlotCount++;
		}
	}
	return FreeSlotCount;
}

int32 UFlareCargoBay::GetUsedCargoSpace() const
{
	int32 Used = 0;

	for (int CargoIndex = 0; CargoIndex < CargoBay.Num(); CargoIndex++)
	{
		Used += CargoBay[CargoIndex].Quantity;
	}

	return Used;
}

int32 UFlareCargoBay::GetFreeCargoSpace() const
{
	return GetCapacity() - GetUsedCargoSpace();
}

int32 UFlareCargoBay::GetResourceQuantitySimple(FFlareResourceDescription* Resource) const
{
	int32 Quantity = 0;
	for (int CargoIndex = 0; CargoIndex < CargoBay.Num(); CargoIndex++)
	{
		const FFlareCargo& Cargo = CargoBay[CargoIndex];
		if (Cargo.Resource == Resource)
		{
			Quantity += Cargo.Quantity;
		}
	}
	return Quantity;
}

int32 UFlareCargoBay::GetResourceQuantity(FFlareResourceDescription* Resource, UFlareCompany* Client, uint8 CheckRestrictionContext, bool IgnoresRestrictionNobody) const
{
	int32 Quantity = 0;

	for (int CargoIndex = 0; CargoIndex < CargoBay.Num() ; CargoIndex++)
	{
		const FFlareCargo& Cargo = CargoBay[CargoIndex];
		if (Cargo.Resource == Resource)
		{
			if(!CheckRestriction(&Cargo, Client, CheckRestrictionContext, IgnoresRestrictionNobody))
			{
				continue;
			}

			Quantity += Cargo.Quantity;
		}
	}

	if((Client && !Client->IsPlayerCompany())  && Game->GetQuestManager() != nullptr)
	{
		int32 ReservedQuantity = Game->GetQuestManager()->GetReservedQuantity(Parent, Resource);
		int32 QuantityAfterReservation = FMath::Max(0, Quantity - ReservedQuantity);
		return QuantityAfterReservation;
	}
	else
	{
		return Quantity;
	}
}

int32 UFlareCargoBay::GetFreeSpaceForResource(FFlareResourceDescription* Resource, UFlareCompany* Client, bool LockOnly, uint8 Context, bool IgnoresRestrictionNobody) const
{
	int32 Quantity = 0;

	for (int CargoIndex = 0; CargoIndex < CargoBay.Num() ; CargoIndex++)
	{
		const FFlareCargo& Cargo = CargoBay[CargoIndex];

		if(!CheckRestriction(&Cargo, Client, Context, IgnoresRestrictionNobody))
		{
			continue;
		}

		if(LockOnly && Cargo.Lock == EFlareResourceLock::NoLock)
		{
			continue;
		}

		if (Cargo.Resource == NULL)
		{
			Quantity += GetSlotCapacity();
		}
		else if (Cargo.Resource == Resource)
		{
			Quantity += GetSlotCapacity() - Cargo.Quantity;
		}
	}


	if((!Client || !Client->IsPlayerCompany()) && Game->GetQuestManager() != nullptr)
	{
		int32 ReservedCapacity = Game->GetQuestManager()->GetReservedCapacity(Parent, Resource);
		int32 CapacityAfterReservation = FMath::Max(0, Quantity - ReservedCapacity);
		return CapacityAfterReservation;
	}
	else
	{
		return Quantity;
	}
}


int32 UFlareCargoBay::GetTotalCapacityForResource(FFlareResourceDescription* Resource, UFlareCompany* Client, bool LockOnly) const
{
	int32 Quantity = 0;

	for (int CargoIndex = 0; CargoIndex < CargoBay.Num() ; CargoIndex++)
	{
		const FFlareCargo& Cargo = CargoBay[CargoIndex];

		if(!CheckRestriction(&Cargo, Client))
		{
			continue;
		}

		if(LockOnly && Cargo.Lock == EFlareResourceLock::NoLock)
		{
			continue;
		}

		if(Cargo.Lock != EFlareResourceLock::NoLock)
		{

		}

		if (Cargo.Resource == NULL || Cargo.Resource == Resource)
		{
			Quantity += GetSlotCapacity();
		}
	}
	return Quantity;
}


bool UFlareCargoBay::HasRestrictions() const
{
	for (int CargoIndex = 0; CargoIndex < CargoBay.Num() ; CargoIndex++)
	{
		const FFlareCargo& Cargo = CargoBay[CargoIndex];
		if(Cargo.Restriction != EFlareResourceRestriction::Everybody)
		{
			return true;
		}
	}
	return false;
}

int32 UFlareCargoBay::GetSlotCount() const
{
	return CargoBayCount;
}

FFlareCargo* UFlareCargoBay::GetSlot(int32 Index)
{
	if(Index >= CargoBay.Num())
	{
		return NULL;
	}

	return &CargoBay[Index];
}

bool UFlareCargoBay::LockSlot(FFlareResourceDescription* Resource, EFlareResourceLock::Type LockType, bool ManualLock)
{
	if(LockType == EFlareResourceLock::NoLock)
	{
		return false;
	}

	//Check double lock
	for(FFlareCargo& Cargo : CargoBay)
	{
		if(Cargo.Resource == Resource && Cargo.Lock != EFlareResourceLock::NoLock)
		{
			FLOG("WARNING: double lock for a cargo bay for a resource. Abort");
			return false;
		}
	}

	//Check slot with the resource
	for(FFlareCargo& Cargo : CargoBay)
	{
		if (Cargo.Resource == Resource)
		{
			Cargo.Lock = LockType;
			Cargo.ManualLock = ManualLock;
			return true;
		}
	}

	//Check empty slots
	for(FFlareCargo& Cargo : CargoBay)
	{
		if (Cargo.Resource == NULL)
		{
			Cargo.Lock = LockType;
			Cargo.ManualLock = ManualLock;
			Cargo.Resource = Resource;
			Cargo.Quantity = 0;
			return true;
		}
	}

	return false;
}

void UFlareCargoBay::HideUnlockedSlots()
{
	for(FFlareCargo& Cargo : CargoBay)
	{
		if(Cargo.Lock == EFlareResourceLock::NoLock)
		{
			Cargo.Lock = EFlareResourceLock::Hidden;
			Cargo.ManualLock = false;
		}
	}
}

void UFlareCargoBay::UnlockAll(bool IgnoreManualLock)
{
	for (int CargoIndex = 0; CargoIndex < CargoBay.Num() ; CargoIndex++)
	{
		FFlareCargo& Cargo = CargoBay[CargoIndex];

		if (Cargo.Lock != EFlareResourceLock::NoLock)
		{
			if(IgnoreManualLock && Cargo.ManualLock)
			{
				continue;
			}

			Cargo.Lock = EFlareResourceLock::NoLock;
			Cargo.ManualLock = false;

			if (Cargo.Quantity == 0)
			{
				Cargo.Resource = NULL;
			}
		}
	}
}

TEnumAsByte<EFlareResourceRestriction::Type> UFlareCargoBay::RotateSlotRestriction(int32 SlotIndex)
{
	if (SlotIndex >= CargoBay.Num())
	{
		FLOGV("Invalid index %d for rotate slot restriction (cargo bay size: %d)", SlotIndex, CargoBay.Num());
	}

	if (CargoBay[SlotIndex].Lock == EFlareResourceLock::Output || CargoBay[SlotIndex].Lock == EFlareResourceLock::Input)
	{
		if (CargoBay[SlotIndex].Restriction == EFlareResourceRestriction::Everybody)
		{
			SetSlotRestriction(SlotIndex, EFlareResourceRestriction::OwnerOnly);
		}
		else if (CargoBay[SlotIndex].Restriction == EFlareResourceRestriction::OwnerOnly)
		{
			SetSlotRestriction(SlotIndex, EFlareResourceRestriction::Nobody);
		}
		else if (CargoBay[SlotIndex].Restriction == EFlareResourceRestriction::Nobody)
		{
			SetSlotRestriction(SlotIndex, EFlareResourceRestriction::Everybody);
		}
	}
	else if (CargoBay[SlotIndex].Lock == EFlareResourceLock::Trade)
		if (CargoBay[SlotIndex].Restriction == EFlareResourceRestriction::Everybody)
		{
			SetSlotRestriction(SlotIndex, EFlareResourceRestriction::OwnerOnly);
		}
		else if (CargoBay[SlotIndex].Restriction == EFlareResourceRestriction::OwnerOnly)
		{
			SetSlotRestriction(SlotIndex, EFlareResourceRestriction::BuyersOnly);
		}
		else if (CargoBay[SlotIndex].Restriction == EFlareResourceRestriction::BuyersOnly)
		{
			SetSlotRestriction(SlotIndex, EFlareResourceRestriction::BuyersOwnerOnly);
		}
		else if (CargoBay[SlotIndex].Restriction == EFlareResourceRestriction::BuyersOwnerOnly)
		{
			SetSlotRestriction(SlotIndex, EFlareResourceRestriction::SellersOnly);
		}
		else if (CargoBay[SlotIndex].Restriction == EFlareResourceRestriction::SellersOnly)
		{
			SetSlotRestriction(SlotIndex, EFlareResourceRestriction::SellersOwnerOnly);
		}
		else if (CargoBay[SlotIndex].Restriction == EFlareResourceRestriction::SellersOwnerOnly)
		{
			SetSlotRestriction(SlotIndex, EFlareResourceRestriction::Nobody);
		}
		else if (CargoBay[SlotIndex].Restriction == EFlareResourceRestriction::Nobody)
		{
			SetSlotRestriction(SlotIndex, EFlareResourceRestriction::Everybody);
		}
	return CargoBay[SlotIndex].Restriction;
}

void UFlareCargoBay::SetSlotRestriction(int32 SlotIndex, EFlareResourceRestriction::Type RestrictionType)
{
	if(SlotIndex >= CargoBay.Num())
	{
		FLOGV("Invalid index %d for set slot restriction (cargo bay size: %d)", SlotIndex, CargoBay.Num());
	}
	CargoBay[SlotIndex].Restriction = RestrictionType;
}

TEnumAsByte<EFlareResourceRestriction::Type> UFlareCargoBay::GetRestriction(int32 SlotIndex)
{
	return CargoBay[SlotIndex].Restriction;
}

bool UFlareCargoBay::WantSell(FFlareResourceDescription* Resource, UFlareCompany* Client, bool RequireStock) const
{
	for (int CargoIndex = 0; CargoIndex < CargoBay.Num() ; CargoIndex++)
	{
		const FFlareCargo& Cargo = CargoBay[CargoIndex];
		if (Cargo.Resource != NULL && Cargo.Resource != Resource)
		{
			continue;
		}

		if (!CheckRestriction(&Cargo, Client, 1))
		{
			continue;
		}

		if (RequireStock && Cargo.Quantity == 0)
		{
			continue;
		}

		if (Cargo.Lock == EFlareResourceLock::NoLock ||
				Cargo.Lock == EFlareResourceLock::Output ||
				Cargo.Lock == EFlareResourceLock::Trade)
		{
			return true;
		}
	}

	return false;
}

bool UFlareCargoBay::WantBuy(FFlareResourceDescription* Resource, UFlareCompany* Client) const
{
	for (int CargoIndex = 0; CargoIndex < CargoBay.Num() ; CargoIndex++)
	{
		const FFlareCargo& Cargo = CargoBay[CargoIndex];
		if(Cargo.Resource != NULL && Cargo.Resource != Resource)
		{
			continue;
		}

		if(!CheckRestriction(&Cargo, Client, 2))
		{
			continue;
		}

		if(Cargo.Lock == EFlareResourceLock::NoLock ||
				Cargo.Lock == EFlareResourceLock::Input||
				Cargo.Lock == EFlareResourceLock::Trade)
		{
			return true;
		}
	}
	return false;
}

bool UFlareCargoBay::CheckRestriction(const FFlareCargo* Cargo, UFlareCompany* Client, uint8 Context, bool IgnoresRestrictionNobody) const
{
	// Check restrictions
	if(Cargo->Restriction == EFlareResourceRestriction::Nobody && IgnoresRestrictionNobody == false)
	{
		// Restricted slot
		return false;
	}

	else if (Cargo->Restriction == EFlareResourceRestriction::OwnerOnly && Client != Parent->GetCompany())
	{
		// Restricted slot
		return false;
	}

	if (Context != 0)
	{
		if ((Cargo->Restriction == EFlareResourceRestriction::BuyersOnly || (Cargo->Restriction == EFlareResourceRestriction::BuyersOwnerOnly && Client != Parent->GetCompany())) && Context != 1)
		{
			//restricted if not trying to sell to another ship
			return false;
		}
		else if ((Cargo->Restriction == EFlareResourceRestriction::SellersOnly || (Cargo->Restriction == EFlareResourceRestriction::SellersOwnerOnly && Client != Parent->GetCompany())) && Context != 2)
		{
			//restricted if not trying to buy from another ship
			return false;
		}
	}

	return true;
}

bool UFlareCargoBay::SortBySlotType(const FSortableCargoInfo& A, const FSortableCargoInfo& B)
{
	return A.Cargo->Lock > B.Cargo->Lock;
}

#undef LOCTEXT_NAMESPACE