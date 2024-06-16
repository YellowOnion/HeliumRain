#pragma once

#include "FlareSpacecraftPawn.h"
#include "FlareWeapon.h"
#include "FlareSpacecraftComponent.h"
#include "FlareSpacecraftSpinningComponent.h"
#include "Subsystems/FlareSpacecraftDamageSystem.h"
#include "Subsystems/FlareSpacecraftNavigationSystem.h"
#include "Subsystems/FlareSpacecraftDockingSystem.h"
#include "Subsystems/FlareSpacecraftWeaponsSystem.h"
#include "FlareSpacecraftStateManager.h"
#include "FlarePilotHelper.h"
#include "../Game/FlareDebrisField.h"
#include "FlareSpacecraft.generated.h"

class UFlareShipPilot;
class AFlareSpacecraft;
class UCanvasRenderTarget2D;


/** Target info */
USTRUCT()
struct FFlareScreenTarget
{
	GENERATED_USTRUCT_BODY()

	AFlareSpacecraft*      Spacecraft;

	float                  DistanceFromScreenCenter;

};


/** Ship class */
UCLASS(Blueprintable, ClassGroup = (Flare, Ship))
class AFlareSpacecraft : public AFlareSpacecraftPawn
{

public:

	GENERATED_UCLASS_BODY()

	/*----------------------------------------------------
		Component data
	----------------------------------------------------*/

	/** Airframe component */
	UPROPERTY(Category = Mesh, VisibleDefaultsOnly, BlueprintReadOnly)
	class UFlareSpacecraftComponent* Airframe;


public:

	/*----------------------------------------------------
		Public methods
	----------------------------------------------------*/

	virtual void BeginPlay() override;

	virtual void TickSpacecraft(float DeltaSeconds) override;
	
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	virtual void LifeSpanExpired() override;

	virtual void Destroyed() override;

	virtual void OnRepaired();

	virtual void OnRefilled();

	virtual void OnDocked(AFlareSpacecraft* DockStation, bool TellUser, FFlareResourceDescription* TransactionResource, uint32 TransactionQuantity, UFlareSimulatedSpacecraft* SourceSpacecraft, UFlareSimulatedSpacecraft* DestinationSpacecraft, bool TransactionDonation = 0, FFlareSpacecraftComponentDescription* TransactionNewPartDesc = NULL, int32 TransactionNewPartWeaponGroupIndex = NULL);

	virtual void OnUndocked(AFlareSpacecraft* DockStation);

	virtual void SetPause(bool Pause);

	virtual void Redock();

	virtual void RedockTo(AFlareSpacecraft* Station);

	virtual float GetSpacecraftMass() const;

	void SetWantUndockInternalShips(bool Set);
	void LaunchRetrieveDrones();

	void CarrierReleaseInternalDockShips(float DeltaSeconds);

	bool GetWantUndockInternalShips();
	bool GetIsDestroyed();

	/*----------------------------------------------------
		Player interface
	----------------------------------------------------*/

	/** Reset the target */
	void ResetCurrentTarget();

	/** Clear target */
	void ClearInvalidTarget(PilotHelper::PilotTarget invalidTarget);

	/** Get the current target */
	PilotHelper::PilotTarget GetCurrentTarget();

	/** Are we scanning for a waypoint ? */
	bool IsInScanningMode();

	/** Get the progress of scanning */
	void GetScanningProgress(bool& AngularIsActive, bool& LinearIsActive, bool& ScanningIsActive,
		float& AngularProgress, float& LinearProgress, float& AnalyzisProgress, float& ScanningDistance, float& ScanningSpeed);

	/** Get the progress in detail for an objective or scannable */
	void GetScanningProgressInternal(const FVector& Location, const float Radius, bool& AngularIsActive, bool& LinearIsActive, bool& ScanningIsActive,
		float& AngularProgress, float& LinearProgress, float& AnalyzisProgress, float& ScanningDistance, float& ScanningSpeed);

	/** Are we done with the scan ? */
	bool IsWaypointScanningFinished() const;

	/** Are we currently docking ? */
	bool GetManualDockingProgress(AFlareSpacecraft*& OutStation, FFlareDockingParameters& OutParameters, FText& OutInfo) const;


	/*----------------------------------------------------
		Ship interface
	----------------------------------------------------*/

	virtual void Load(UFlareSimulatedSpacecraft* Parent);

	virtual void Save();

	virtual void FinishLoadandReady();

	virtual void SetOwnerCompany(UFlareCompany* Company);
	
	virtual UFlareInternalComponent* GetInternalComponentAtLocation(FVector Location) const;
	
	virtual UFlareSpacecraftDamageSystem* GetDamageSystem() const;

	virtual UFlareSpacecraftNavigationSystem* GetNavigationSystem() const;
	
	virtual UFlareSpacecraftDockingSystem* GetDockingSystem() const;

	virtual UFlareSpacecraftWeaponsSystem* GetWeaponsSystem() const;

	/** Set asteroid data from an asteroid save */
	void SetAsteroidData(FFlareAsteroidSave* Data);

	/** Try to attach to the parent world object */
	void TryAttachParentActor();

	/** Try to attach to the parent complex station */
	void TryAttachParentComplex();

	/** Apply the current asteroid data */
	void ApplyAsteroidData();

	void UpdateDynamicComponents();
	
	UFlareSimulatedSector* GetOwnerSector();
	
	void SetCurrentTarget(PilotHelper::PilotTarget const& Target);

	void UpdateSpacecraftPawn(float DeltaSeconds);

	void SafeHide(bool ShowExplosion = true);

	void SignalIncomingBombsDeadTarget();

	/** Slower actor destruction*/
	void SafeDestroy();

	void FinishAutoPilots();

	void FinishSafeDestroy();

	bool IsSafeEither();

	bool IsSafeDestroying();

	FFlareSpacecraftComponentDescription* GetDefaultWeaponFallback(bool IsFinalExplosion);
	float GetExplosionScaleFactor(FFlareSpacecraftComponentDescription* WeaponFallback, bool IsFinalExplosion);

	bool CheckIsExploding();

public:

	/*----------------------------------------------------
		Customization
	----------------------------------------------------*/

	/** Set the description to use for all orbital engines */
	virtual void SetOrbitalEngineDescription(FFlareSpacecraftComponentDescription* Description);

	/** Set the description to use for all RCS */
	virtual void SetRCSDescription(FFlareSpacecraftComponentDescription* Description);

	virtual void UpdateCustomization() override;

	virtual void StartPresentation() override;

	/** Canvas callback for the ship name */
	UFUNCTION()
	void DrawShipName(UCanvas* TargetCanvas, int32 Width, int32 Height);
	
public:

	/*----------------------------------------------------
		Input methods
	----------------------------------------------------*/

	virtual void SetupPlayerInputComponent(class UInputComponent* InputComponent) override;

	virtual void StartFire();

	virtual void StopFire();

	virtual void LeftMousePress();

	virtual void LeftMouseRelease();

	virtual void DeactivateWeapon();

	virtual void ActivateWeaponGroup1();

	virtual void ActivateWeaponGroup2();

	virtual void ActivateWeaponGroup3();

	virtual void ActivateWeaponGroupByIndex(int32 Index);

	virtual void NextWeapon();

	virtual void PreviousWeapon();

	virtual void NextTarget();

	virtual void PreviousTarget();

	virtual void AlternateNextTarget();

	virtual void AlternatePreviousTarget();


	virtual void YawInput(float Val);

	virtual void PitchInput(float Val);

	virtual void RollInput(float Val);

	virtual void ThrustInput(float Val);

	virtual void MoveVerticalInput(float Val);

	virtual void MoveHorizontalInput(float Val);



	virtual void GamepadMoveVerticalInput(float Val);

	virtual void GamepadMoveHorizontalInput(float Val);

	virtual void GamepadThrustInput(float Val);

	virtual void GamepadYawInput(float Val);

	virtual void GamepadPitchInput(float Val);


	virtual void JoystickYawInput(float Val);

	virtual void JoystickPitchInput(float Val);

	virtual void JoystickRollInput(float Val);

	virtual void JoystickThrustInput(float Val);

	virtual void JoystickMoveVerticalInput(float Val);

	virtual void JoystickMoveHorizontalInput(float Val);


	virtual void ZoomIn();

	virtual void ZoomOut();

	virtual void CombatZoomIn();

	virtual void CombatZoomOut();

	virtual void FaceForward();

	virtual void FaceBackward();

	virtual void Brake();

	virtual void BrakeToVelocity(const FVector& VelocityTarget = FVector::ZeroVector);

	virtual void LockDirectionOn();

	virtual void LockDirectionOff();

	virtual void FindTarget();

	bool GetAttachedToParentActor();

protected:
	/*----------------------------------------------------
		Internal data
	----------------------------------------------------*/

	// Component descriptions, save data
	UPROPERTY()
	UFlareSimulatedSpacecraft*	                   Parent;

	FFlareSpacecraftComponentDescription*          OrbitalEngineDescription;
	FFlareSpacecraftComponentDescription*          RCSDescription;
	FVector                                        SmoothedVelocity;
	
	// Idle shipyard dynamic component
	UPROPERTY()
	UClass*                                        IdleShipyardTemplate;

	// Weapon loaded
	UPROPERTY()
	USoundCue*                                     WeaponLoadedSound;

	// Weapon loaded
	UPROPERTY()
	USoundCue*                                     WeaponUnloadedSound;

	// Lifesupport status
	UPROPERTY()
	UFlareSpacecraftComponent*                     ShipCockit;

	// Pilot object
	UPROPERTY()
	UFlareShipPilot*                               Pilot;

	// Decal material
	UPROPERTY()
	UMaterialInstanceDynamic*                      DecalMaterial;

	// Decal material
	UPROPERTY()
	UMaterialInstanceDynamic*                      ShipNameDecalMaterial;

	// Ship name texture
	UPROPERTY()
	UCanvasRenderTarget2D*                         ShipNameTexture;

	// Ship name font
	UPROPERTY()
	UFont*                                         ShipNameFont;

	// Systems
	UPROPERTY()
	UFlareSpacecraftDamageSystem*                  DamageSystem;
	UPROPERTY()
	UFlareSpacecraftNavigationSystem*              NavigationSystem;
	UPROPERTY()
	UFlareSpacecraftDockingSystem*                 DockingSystem;
	UPROPERTY()
	UFlareSpacecraftWeaponsSystem*                 WeaponsSystem;
	UPROPERTY()
	UFlareSpacecraftStateManager*				   StateManager;

	bool                                           HasExitedSector;
	bool                                           Paused;
	bool                                           LoadedAndReady;
	
	bool                                           AttachedToParentActor;

	bool                                           IsScanningWaypoint;
	float                                          ScanningTimer;
	float                                          ScanningTimerDuration;
	
	// Docking info
	bool                                           IsManualDocking;
	bool                                           IsAutoDocking;
	FFlareDockingParameters                        ManualDockingStatus;
	FFlareDockingInfo                              ManualDockingInfo;
	AFlareSpacecraft*                              ManualDockingTarget;

	float										   TimeBeforeNextInternalUndock;
	float										   TimeSinceUncontrollable;

	FFlareMovingAverage<float>                     JoystickRollInputVal;

	bool										   HasUndockedAllInternalShips;
	bool										   IsSafeDestroyingRunning;
	bool										   BegunSafeDestroy;

	TArray<AFlareSpacecraft*>					   InSectorSquad;
	TArray<AFlareBomb*>							   IncomingBombs;

	/*----------------------------------------------------
		Target selection
	----------------------------------------------------*/

	// Target spacecraft
	PilotHelper::PilotTarget                       CurrentTarget;

	// target spacecraft index in the list
	int32                                          TargetIndex;

	// Time elapsed since we last selected
	float                                          TimeSinceSelection;

	// Max time between target selections before "prev" or "next" resets to center
	float                                          MaxTimeBeforeSelectionReset;

	// Throttle memory
	float                                          PreviousJoystickThrottle;

	TArray<FFlareScreenTarget> Targets;

	TArray<FFlareScreenTarget>& GetCurrentTargets();

	mutable bool TimeToStopCached = false;
	mutable float TimeToStopCache;

	float										   TimeSinceLastExplosion;
	int32										   ExplodingTimes;
	int32										   ExplodingTimesMax;
	bool										   IsExploding;

public:
	TArray<AFlareBomb*> GetIncomingBombs();
	int32 GetIncomingActiveBombQuantity();
	void TrackIncomingBomb(AFlareBomb* Bomb);
	void UnTrackIncomingBomb(AFlareBomb* Bomb);
	void UnTrackAllIncomingBombs();

	void AddToInsectorSquad(AFlareSpacecraft* Adding);
	void RemoveFromInsectorSquad(AFlareSpacecraft* Removing);
	
	void SetUndockedAllShips(bool Set);

	void BeginExplodingShip();
	void ExplodingShip();

	/*----------------------------------------------------
		Getters
	----------------------------------------------------*/

	TArray<AFlareSpacecraft*> GetInSectorSquad() const
	{
		return InSectorSquad;
	}

	bool GetHasUndockedInternalShips() const
	{
		return HasUndockedAllInternalShips;
	}

	bool GetWantUndockInternalShips() const
	{
		if (GetParent())
		{
			return GetParent()->GetData().WantUndockInternalShips;
		}
		return false;
	}

	bool GetIsManualDocking() const
	{
		return IsManualDocking;
	}

	bool GetIsAutoDocking() const
	{
		return IsAutoDocking;
	}

	FText GetShipStatus() const;

	/** Return linear velocity in meters */
	FVector GetLinearVelocity() const;

	float GetTimeToStop() const;

	inline UFlareSimulatedSpacecraft* GetParent() const
	{
		return Parent;
	}

	bool IsOutsideSector() const;

	inline bool IsLoadedAndReady() const
	{
		return LoadedAndReady;
	}

	inline FFlareSpacecraftDescription* GetDescription() const
	{
		return Parent->GetDescription();
	}

	bool IsComplexElement() const;

	AFlareSpacecraft* GetComplex() const;

	inline UFlareCompany* GetCompany()
	{
		return Parent->GetCompany();
	}

	inline FName GetImmatriculation() const
	{
		return Parent->GetImmatriculation();
	}

	inline EFlarePartSize::Type GetSize()
	{
		return Parent->GetSize();
	}

	inline bool IsMilitary()
	{
		return Parent->IsMilitary();
	}

	inline bool IsMilitaryArmed()
	{
		return Parent->IsMilitaryArmed();
	}

	inline bool IsNotMilitary()
	{
		return Parent->IsNotMilitary();
	}

	inline bool IsCapableCarrier()
	{
		return Parent->IsCapableCarrier();
	} 

	inline FFlareSpacecraftSave& GetData()
	{
		return Parent->GetData();
	}

	inline FFlareSpacecraftSave const& GetConstData() const
	{
		return Parent->GetData();
	}

	bool IsStation() const
	{
		return Parent->IsStation();
	}

	bool IsUncapturable() const
	{
		return Parent->IsUncapturable();
	}

	bool IsImmobileStation() const
	{
		return Parent->IsImmobileStation();
	}

	bool IsHostile(UFlareCompany* TargetCompany, bool UseCache = false) const
	{
		return Parent->IsHostile(TargetCompany, UseCache);
	}

	bool IsPlayerHostile() const;

	/*inline FText GetNickName() const override
	{
		return ShipData.NickName;
	}*/

	inline FFlareSpacecraftComponentDescription* GetOrbitalEngineDescription() const
	{
		return OrbitalEngineDescription;
	}

	inline FFlareSpacecraftComponentDescription* GetRCSDescription() const
	{
		return RCSDescription;
	}

	virtual UFlareSpacecraftComponent* GetCockpit() const
	{
		return ShipCockit;
	}

	virtual UCameraComponent* GetCamera() const
	{
		return Cast<UCameraComponent>(Camera);
	}

	inline UFlareSpacecraftStateManager* GetStateManager() const
	{
		return StateManager;
	}

	inline UFlareShipPilot* GetPilot() const
	{
		return Pilot;
	}

	inline bool IsMovingForward() const
	{
		return (FVector::DotProduct(GetSmoothedLinearVelocity(), GetFrontVector()) > 0);
	}

	inline FVector GetSmoothedLinearVelocity() const
	{
		return SmoothedVelocity;
	}

	inline FVector GetFrontVector() const
	{
		return GetActorRotation().RotateVector(FVector(1,0,0));
	}

	inline FVector GetUpVector() const
	{
		return GetActorRotation().RotateVector(FVector(0, 0, 1));
	}

	inline bool IsPaused()
	{
		return Paused;
	}

	float GetTimeSinceUncontrollable() const
	{
		return TimeSinceUncontrollable;
	}

	float GetPreferedAnticollisionTime() const;

	float GetAgressiveAnticollisionTime() const;
};
