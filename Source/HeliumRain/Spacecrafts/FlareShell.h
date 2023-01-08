#pragma once

#include "FlareWeapon.h"
#include "FlareShell.generated.h"

UCLASS(Blueprintable, ClassGroup = (Flare, Ship), meta = (BlueprintSpawnableComponent))
class AFlareShell : public AActor
{
public:

	GENERATED_UCLASS_BODY()

public:

	/*----------------------------------------------------
		Public methods
	----------------------------------------------------*/

	/** Properties setup */
	void Initialize(class UFlareWeapon* Weapon, const FFlareSpacecraftComponentDescription* Description, FVector ShootDirection, FVector ParentVelocity, bool Tracer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void LifeSpanExpired() override;

	virtual void SetPause(bool Pause);

	/** Impact happened */
	UFUNCTION()
	void OnImpact(const FHitResult& HitResult, const FVector& ImpactVelocity);

	virtual void DetonateAt(FVector DetonatePoint);
	virtual void ProcessCurrentDetonations();

	bool Trace(const FVector& Start, const FVector& End, FHitResult& HitOut);

	virtual float ApplyDamage(AActor *ActorToDamage, UPrimitiveComponent* ImpactComponent, FVector ImpactLocation,  FVector ImpactAxis,  FVector ImpactNormal, float ImpactPower, float ImpactRadius, EFlareDamage::Type DamageType);
	//	virtual void Destroyed() override;

	FTraceHandle RequestTrace(const FVector& Start, const FVector& End);
	void OnTraceCompleted(const FTraceHandle& Handle, FTraceDatum& Data);
	void DoWorkWithTraceResults(const FTraceDatum& TraceData);

	virtual void SetFuzeTimer(float TargetSecureTime, float TargetActiveTime);

	virtual void CheckFuze(FVector ActorLocation, FVector NextActorLocation);

	virtual void SafeDestroy();

	virtual void FinishSafeDestroy();
	virtual void UnsetSafeDestroyed();

protected:
	/*----------------------------------------------------
		Protected data
	----------------------------------------------------*/

	FTraceHandle LastTraceHandle;
	FTraceDelegate TraceDelegate;

	FHitResult								PreviousHitResult;

	/** Impact sound */
	UPROPERTY()
	USoundCue*                               ImpactSound;

	/** Damage sound */
	UPROPERTY()
	USoundCue*                               DamageSound;

	/** Mesh component */
	UPROPERTY()
	USceneComponent*                         ShellComp;

	/** Special effects on explosion */
	UPROPERTY()
	UParticleSystem*                         ExplosionEffectTemplate;

	/** Special effects on impact */
	UPROPERTY()
	UParticleSystem*                         ImpactEffectTemplate;

	/** Special effects on flight */
	UPROPERTY()
	UParticleSystem*                         FlightEffectsTemplate;

	// Flight effects
	UPROPERTY()
	UParticleSystemComponent*                FlightEffects;

	/** Burn mark decal */
	UPROPERTY()
	UMaterialInterface*                      ExplosionEffectMaterial;

	float                                    ExplosionEffectScale;
	float                                    ImpactEffectScale;

	// Shell data
	FVector                                        ShellVelocity;
	const FFlareSpacecraftComponentDescription*    ShellDescription;
	FVector                                        LastLocation;	
	float                                          ShellMass;
	bool                                           TracerShell;
	bool                                           Armed;
	float                                          MinEffectiveDistance;
	float                                          SecureTime;
	float                                          ActiveTime;

	// References
	UPROPERTY()
	class UFlareWeapon*                            ParentWeapon;

	class AFlarePlayerController*                  PC;
	bool                                           ManualTurret;
	UFlareSector*								   LocalSector;

	bool										   IsSafeDestroyingRunning;
	bool										   SafeDestroyed;

	bool										   IsDetonating;
	FVector										   DetonationPoint;
	TArray<AFlareSpacecraft*>					   DetonationSpacecraftCandidates;
	TArray<AFlareMeteorite*>	   				   DetonationMeteoriteCandidates;
	TArray<AFlareBomb*>							   DetonationBombCandidates;
};
